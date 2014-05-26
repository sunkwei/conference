#include "ZonekeyConference.h"

ZonekeyConference::ZonekeyConference(ZonekeyConferenceMode mode)
	: mode_(mode)
{
	sink_id_ = 0;
	stream_id_ = 0;
}

ZonekeyConference::~ZonekeyConference(void)
{
}

ZonekeyStream *ZonekeyConference::add_stream(KVS &params)
{
	char info[1024];
	if (!chk_params(params, info, "client_ip", "client_port", "payload", 0)) {
		fprintf(stderr, "[Conference] %s: params chk err: '%s'\n", __FUNCTION__, info);
		return 0;
	}

	const char *client_ip = params["client_ip"].c_str();
	int client_port = atoi(params["client_port"].c_str());
	int payload_type = atoi(params["payload"].c_str());

	/** 创建双向 RtpSession 

	 */
	RtpSession *rs = rtp_session_new(RTP_SESSION_SENDRECV);
	rtp_session_set_payload_type(rs, payload_type);
	rtp_session_set_remote_addr_and_port(rs, client_ip, client_port, client_port+1);
	rtp_session_set_local_addr(rs, util_get_myip(), 0, 0);

	// FIXME: 如果 h264，则缺省的 jitter buffer 设置太小，将导致严重丢包
	if (payload_type == 100) {
		JBParameters jbp;
		jbp.adaptive = 1;
		jbp.max_packets = 500;
		jbp.max_size = -1;
		jbp.min_size = jbp.nom_size = 200; // 200ms

		rtp_session_set_jitter_buffer_params(rs, &jbp);
	}

	stream_id_ ++;
	if (stream_id_ > 0x7ffffffe)
		stream_id_ = 0;

	ZonekeyStream *s = createStream(rs, params, stream_id_);
	if (!s) {
		fprintf(stderr, "[Conference] %s: createStream ERR\n", __FUNCTION__);
		rtp_session_destroy(rs);
		return 0;
	}

	// 根据 id 保存
	map_streams_[s->id()] = s;
	return s;
}

void ZonekeyConference::del_stream(ZonekeyStream *s)
{
	RtpSession *rs = s->rtp_session();
	freeStream(s);
	rtp_session_destroy(rs);

	STREAMS::iterator itf = map_streams_.find(s->id());
	if (itf != map_streams_.end()) {
		map_streams_.erase(itf);
	}
}

ZonekeyStream *ZonekeyConference::get_stream(int id)
{
	STREAMS::iterator itf = map_streams_.find(id);
	if (itf != map_streams_.end()) {
		return itf->second;
	}
	return 0;
}

ZonekeySink *ZonekeyConference::add_sink(KVS &params)
{
	// 创建仅仅发送的 rtp session，要求 client_ip, client_port, payload type
	char info[1024];
	if (!chk_params(params, info, "client_ip", "client_port", "payload", 0)) {
		fprintf(stderr, "[Conference] %s: params chk err: '%s'\n", __FUNCTION__, info);
		return 0;
	}

	const char *client_ip = params["client_ip"].c_str();
	int client_port = atoi(params["client_port"].c_str());
	int payload_type = atoi(params["payload"].c_str());

	RtpSession *rs = rtp_session_new(RTP_SESSION_SENDONLY);
	rtp_session_set_payload_type(rs, payload_type);
	rtp_session_set_remote_addr_and_port(rs, client_ip, client_port, client_port+1);
	rtp_session_set_local_addr(rs, util_get_myip(), 0, 0);

	// FIXME: 如果 h264，则缺省的 jitter buffer 设置太小，将导致严重丢包
	if (payload_type == 100) {
		JBParameters jbp;
		jbp.adaptive = 1;
		jbp.max_packets = 500;
		jbp.max_size = -1;
		jbp.min_size = jbp.nom_size = 200; // 200ms

		rtp_session_set_jitter_buffer_params(rs, &jbp);
	}

	sink_id_++;
	if (sink_id_ > 0x7ffffff0)
		sink_id_ = 0;
	ZonekeySink *s = createSink(rs, params, sink_id_);
	if (!s) {
		fprintf(stderr, "[Conference] %s: createSink err\n", __FUNCTION__);
		rtp_session_destroy(rs);
		return 0;
	}

	map_sinks_[s->id()] = s;
	return s;
}

void ZonekeyConference::del_sink(ZonekeySink *s)
{
	RtpSession *rs = s->rtp_session();
	freeSink(s);
	rtp_session_destroy(rs);

	SINKS::iterator itf = map_sinks_.find(s->id());
	if (itf != map_sinks_.end()) {
		map_sinks_.erase(itf);

	}
}

ZonekeySink *ZonekeyConference::get_sink(int id)
{
	SINKS::iterator itf = map_sinks_.find(id);
	if (itf != map_sinks_.end()) {
		return itf->second;
	}
	return 0;
}
