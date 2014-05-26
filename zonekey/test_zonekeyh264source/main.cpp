#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <cc++/thread.h>
#include <mediastreamer2/mediastream.h>
#include <mediastreamer2/msfilter.h>
#include <mediastreamer2/msticker.h>
#include <mediastreamer2/msrtp.h>
#include <mediastreamer2/zk.h264.source.h>
#include <zonekey/zqpsource.h>
#include <inttypes.h>
#include "../zonekey_video_mixer/X264.h"
#include <mediastreamer2/zk.yuv_sink.h>
#include <zonekey/xmpp_uac.h>

extern "C" {
	__declspec(dllimport) void zonekey_h264_source_register();
}

static const char *get_domain()
{
	const char *domain = "app.zonekey.com.cn";
	char *p = getenv("xmpp_domain");
	if (p)
		domain = p;

	return domain;
}

// 返回 mcu jid
static const char *get_mcu_jid()
{
	static std::string _jid;

	if (_jid.empty()) {
		std::stringstream ss;
		ss << "mse_s_000000000000_mcu_1" << "@" << get_domain();
		_jid = ss.str();
	}

	return _jid.c_str();
}

static const char *get_user_jid()
{
	static std::string _jid;
	if (_jid.empty()) {
		std::stringstream ss;
		ss << "normaluser" << "@" << get_domain();
		_jid = ss.str();
	}

	return _jid.c_str();
}

#include <vector>
#include <assert.h>
#include <map>

typedef std::map<std::string, std::string> KVS;
KVS util_parse_options(const char *options)
{
	KVS kvs;
	std::vector<char *> strs;
	if (!options) return kvs;

	// 使用 strtok() 分割
	char *tmp = strdup(options);
	char *p = strtok(tmp, "&");
	while (p) {
		strs.push_back(p);
		p = strtok(0, "&");
	}

	for (std::vector<char*>::iterator it = strs.begin(); it != strs.end(); ++it) {
		char *key = strtok(*it, "=");
		assert(key);

		const char *value = strtok(0, "\1");	// FIXME:
		if (!value)
			value = "";

		kvs[key] = value;
	}

	free(tmp);
	return kvs;
}

bool chk_params(const KVS &kvs, char info[1024], const char *key, ...)
{
	bool ok = true;
	va_list list;
	va_start(list, key);
	strcpy(info, "");

	while (key) {
		KVS::const_iterator itf = kvs.find(key);
		if (itf == kvs.end()) {
			ok = false;
			snprintf(info, 1024, "'%s' not found", key);
			break;
		}

		key = va_arg(list, const char*);
	}
	va_end(list);

	return ok;	// 都存在
}

const char *util_get_myip()
{
	static std::string _ip;
	if (_ip.empty()) {
		char hostname[512];
		gethostname(hostname, sizeof(hostname));

		struct hostent *host = gethostbyname(hostname);
		if (host) {
			if (host->h_addrtype == AF_INET) {
				in_addr addr;
				
				for (int i = 0; host->h_addr_list[i]; i++) {
					addr.s_addr = *(ULONG*)host->h_addr_list[i];
					_ip = inet_ntoa(addr);
					if (strstr(_ip.c_str(), "192.168.56."))	// virtual box 的ip地址，需要跳过
						continue;
				}
			}
		}
	}

	return _ip.c_str();
}

static uac_token_t *_uac = 0;
static int _sid = -1;
static std::string _ip;
static int _rtp_port, _rtcp_port;
// 是否为 streaming mode
static bool _stream_mode = false;
static HANDLE _env = 0;

static void cb_response(zk_xmpp_uac_msg_token *token, const char *result, const char *option)
{
	if (!strcmp(token->cmd, "test.fc.add_source")) {
		fprintf(stderr, "==test.fc.add_source=== result='%s', options='%s'\n", result, option);

		if (!strcmp(result, "ok") && option) {
			KVS params = util_parse_options(option);
			char info[1024];
			assert(chk_params(params, info, "sid", "server_ip", "server_rtp_port", "server_rtcp_port", 0));

			_sid = atoi(params["sid"].c_str());
			_ip = params["server_ip"].c_str();
			_rtp_port = atoi(params["server_rtp_port"].c_str());
			_rtcp_port = atoi(params["server_rtcp_port"].c_str());

			fprintf(stdout, "ok: the new source:\n\tsid=%d\n\tserver_ip=%s\n\tserver_rtp_port=%d\n\tserver_rtcp_port=%d\n\n",
				_sid, _ip.c_str(), _rtp_port, _rtcp_port);

		}
		else {
			
		}

		SetEvent(_env);
	}
	else if (!strcmp(token->cmd, "test.dc.add_stream")) {
		fprintf(stderr, "== test.dc.add_stream == result='%s', options='%s'\n", result, option);

		if (!strcmp(result, "ok") && option) {
			KVS params = util_parse_options(option);
			char info[1024];
			assert(chk_params(params, info, "streamid", "server_ip", "server_rtp_port", "server_rtcp_port", 0));

			_sid = atoi(params["streamid"].c_str());
			_ip = params["server_ip"];
			_rtp_port = atoi(params["server_rtp_port"].c_str());
			_rtcp_port = atoi(params["server_rtcp_port"].c_str());

			fprintf(stderr, "ok: the new stream: \n"
				"\tstream id=%d\n"
				"\tserver_ip=%s\n"
				"\tserver_rtp_port=%d\n"
				"\tserver_rtcp_port=%d\n\n"
				, _sid, _ip.c_str(), _rtp_port, _rtcp_port);
		}
		else {
			fprintf(stderr, " err: ...\n");
		}

		SetEvent(_env);
	}
	else if (!strcmp(token->cmd, "test.dc.del_stream")) {
		fprintf(stderr, "== test.dc.del_stream == result='%s', options='%s'\n", result, option);
		SetEvent(_env);
	}
	else if (!strcmp(token->cmd, "test.fc.del_source")) {
		fprintf(stderr, "==test.fc.del_source=== result='%s', options='%s'\n", result, option);

		SetEvent(_env);
	}
	else {
		fprintf(stderr, ">>>> cmd=%s, options=%s, result=%s, result_options=%s\n",
			token->cmd, token->option, result, option);
	}
}

static std::string _url;
static void cb_connect_notify(uac_token_t *const token, int is_connected, void *userdata)
{
	fprintf(stderr, "xmpp user login %d\n", is_connected);

	// 发送新建源的请求：
	if (is_connected) {
		char options[128];
		snprintf(options, sizeof(options), "payload=100&desc=h264 from jp100hd %s", _url.c_str());

		if (_stream_mode) {
			fprintf(stderr, "to call 'test.dc.add_stream', add h264 stream for %s\n", _url.c_str());
			zk_xmpp_uac_send_cmd(_uac, get_mcu_jid(), "test.dc.add_stream", options, 0, cb_response);
		}
		else {
			fprintf(stderr, "to call 'test.fc.add_source', and h264 source for %s\n", _url.c_str());
			zk_xmpp_uac_send_cmd(_uac, get_mcu_jid(), "test.fc.add_source", options, 0, cb_response);
		}
	}
	else {
		SetEvent(_env);
	}
}

static bool _quit = false;
static BOOL WINAPI signal_ctrl_c(DWORD code)
{
	_quit = true;
	return 1;
}

int main(int argc, char **argv)
{
	ortp_init();
	ms_init();
	zk_xmpp_uac_init();
	ortp_set_log_level_mask(ORTP_MESSAGE);

	if (argc < 2) {
		fprintf(stderr, "usage: %s <zqpkt src url> [s]\n", argv[0]);
		return -1;
	}

	bool stream_mode = false;
	if (argc == 3 && argv[2][0] == 's')
		stream_mode = true;

	_stream_mode = stream_mode;

	if (stream_mode)
		fprintf(stdout, "=== STREAMING MODE ===\n\n");
	else
		fprintf(stdout, "=== SOURCING MODE ===\n\n");

	_url = argv[1];
	_env = CreateEvent(0, 0, 0, 0);

	fprintf(stdout, "%s: using zqpkt src '%s', just wait mcu .....\n", argv[0], argv[1]);

	// 使用 normaluser 登录
	cb_xmpp_uac cbs = { 0, 0, 0, 0, cb_connect_notify };
	_uac = zk_xmpp_uac_log_in(get_user_jid(), "ddkk1212", &cbs, 0);

	WaitForSingleObject(_env, 10000);

	if (_sid == -1) {
		fprintf(stderr, ":( somthing err, exit!\n");
	}
	else {
		SetConsoleCtrlHandler(signal_ctrl_c, 1);
		
		const char *src_url = argv[1];
		const char *target_ip = _ip.c_str();
		int target_port = _rtp_port;
		int target_port2 = _rtcp_port;

		//fprintf(stdout, "target ip=%s\ntarget port=%d\n\n", target_ip, target_port);

		// only support h264
		rtp_profile_set_payload(&av_profile,100, &payload_type_h264);

		/// 使用 zonekey.h264.source filter
		zonekey_h264_source_register();
		MSFilterDesc *desc = ms_filter_lookup_by_name("ZonekeyH264Source");
		MSFilter *source = ms_filter_new_from_desc(desc);

		if (_stream_mode)
			zonekey_yuv_sink_register();

		// 获取 writer_params
		ZonekeyH264SourceWriterParam writer_param;
		ms_filter_call_method(source, ZONEKEY_METHOD_H264_SOURCE_GET_WRITER_PARAM, &writer_param);

		// RTP Session
		RtpSession *rtpsess = rtp_session_new(RTP_SESSION_SENDRECV);	// 
		rtp_session_set_local_addr(rtpsess, "0.0.0.0", -1, -1);	// 随机端口
		rtp_session_set_remote_addr_and_port(rtpsess, target_ip, target_port, target_port2);
		rtp_session_set_payload_type(rtpsess, 100);	// h264

		JBParameters jb;
		jb.adaptive = 1;
		jb.max_packets = 3000;
		jb.max_size = -1;
		jb.min_size = jb.nom_size = 300;
		rtp_session_set_jitter_buffer_params(rtpsess, &jb);
	
		// disable video jitter control
		rtp_session_enable_jitter_buffer(rtpsess, 0);

		/// rtp sender
		MSFilter *rtp_sender = ms_filter_new(MS_RTP_SEND_ID);
		ms_filter_call_method(rtp_sender, MS_RTP_SEND_SET_SESSION, rtpsess);

		// connect source --> rtp sender
		ms_filter_link(source, 0, rtp_sender, 0);

		// MSTicker
		MSTicker *ticker = ms_ticker_new();

		// attach ticker
		ms_ticker_attach(ticker, source);

		if (_stream_mode) {
			// FIXME: recv, but ....
			MSFilter *rtp_recver = ms_filter_new(MS_RTP_RECV_ID);
			ms_filter_call_method(rtp_recver, MS_RTP_RECV_SET_SESSION, rtpsess);

			MSFilter *decoder = ms_filter_new(MS_H264_DEC_ID);
			MSFilter *sink = ms_filter_new_from_name("ZonekeyYUVSink");

			ms_filter_link(rtp_recver, 0, decoder, 0);
			ms_filter_link(decoder, 0, sink, 0);

			MSTicker *tk = ms_ticker_new();
			//ms_ticker_attach(tk, rtp_recver);
		}

		// 利用 libzqpkt 接收 h264 数据，并且调用 zonekey h264 source 的 writer() 
		void *zqp = 0;
		if (zqpsrc_open(&zqp, src_url) < 0) {
			fprintf(stderr, "to open src err\n");
			return -1;
		}

		while (!_quit) {
			zq_pkt *pkt = zqpsrc_getpkt(zqp);
			if (pkt) {
				if (pkt->type == 1) {
					// h264
					writer_param.write(writer_param.ctx, pkt->ptr, pkt->len, pkt->pts / 45000.0);
				}

				zqpsrc_freepkt(zqp, pkt);
			}
			else
				break;
		}

		// 发送删除 sid 的命令
		char options[128], *cmd="test.fc.del_source";
		if (_stream_mode) {
			snprintf(options, sizeof(options), "streamid=%d", _sid);
			cmd = "test.dc.del_stream";
		}
		else
			snprintf(options, sizeof(options), "sid=%d", _sid);

		zk_xmpp_uac_send_cmd(_uac, get_mcu_jid(), cmd, options, 0, cb_response);
		fprintf(stderr, "\n\nen. to del sid=%d\n\n", _sid);

		zqpsrc_close(zqp);
		fprintf(stderr, "END!\n");

		WaitForSingleObject(_env, 3000);	// 等待 test.fc.de_source 发送成功
	}

	return 0;
}
