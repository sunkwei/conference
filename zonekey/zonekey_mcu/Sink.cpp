#include "Sink.h"
#include "util.h"
#include <assert.h>
#include "log.h"
#include "main_def.h"

Sink::Sink(int id, int pt, const char *desc, const char *who)
	: id_(id)
	, pt_(pt)
	, desc_(desc)
	, who_(who)
	, death_(false)
{
	memset(&stat_, 0, sizeof(stat_));

	rtp_session_ = rtp_session_new(RTP_SESSION_SENDONLY);
	if (pt == 100) {
		rtp_session_set_rtp_socket_send_buffer_size(rtp_session_, 1024*1024);	// for HD bitrate ....
		rtp_session_set_rtp_socket_recv_buffer_size(rtp_session_, 1024*1024);
	}
	else {
		rtp_session_set_rtp_socket_send_buffer_size(rtp_session_, 32*1024);	// 
		rtp_session_set_rtp_socket_recv_buffer_size(rtp_session_, 32*1024);
	}
	rtp_session_set_payload_type(rtp_session_, pt);
	//rtp_session_set_local_addr(rtp_session_, util_get_myip_real(), 0, 0);	// 使用随机端口
	rtp_session_set_local_addr(rtp_session_, util_get_myip_real(), 0, 0);

	// 禁用 jitterbuffer
	rtp_session_enable_jitter_buffer(rtp_session_, 0);
	rtp_session_enable_adaptive_jitter_compensation(rtp_session_, 0);

	evq_ = ortp_ev_queue_new();
	rtp_session_register_event_queue(rtp_session_, evq_);

	time_last_rtcp_ = util_time_now();
}

Sink::~Sink(void)
{
	rtp_session_destroy(rtp_session_);
	ortp_ev_queue_destroy(evq_);
}

bool Sink::death() const
{
	/** 当持续1分钟，收不到对方的 rtcp包，可以认为对方已经结束
	 */

	return death_;
}

const char *Sink::desc()
{
	return desc_.c_str();
}

const char *Sink::who()
{
	return who_.c_str();
}

SinkStat Sink::stat()
{
	return stat_;
}

RtpSession *Sink::get_rtp_session()
{
	return rtp_session_;
}

int Sink::payload_type()
{
	return pt_;
}

int Sink::sink_id()
{
	return id_;
}

const char *Sink::server_ip()
{
	return util_get_myip();
}

int Sink::server_rtcp_port()
{
	int sock = rtp_session_get_rtcp_socket(rtp_session_);
	sockaddr_in sin;
	memset(&sin, 0, sizeof(sin));
	socklen_t len = sizeof(sin);
	getsockname(sock, (sockaddr*)&sin, &len);
	return ntohs(sin.sin_port);
}

int Sink::server_rtp_port()
{
	return rtp_session_get_local_port(rtp_session_);
}

void Sink::run()
{
	if (death_) return;

	// to chk rtcp evq ....
	assert(evq_);
	OrtpEvent *ev = ortp_ev_queue_get(evq_);
	while (ev) {
		OrtpEventType type = ortp_event_get_type(ev);
		if (type == ORTP_EVENT_RTCP_PACKET_RECEIVED) {
			process_rtcp(ortp_event_get_data(ev)->packet);
			time_last_rtcp_ = util_time_now();
		}
		else if (type == ORTP_EVENT_RTCP_PACKET_EMITTED) {
		}
		else if (type == ORTP_EVENT_STUN_PACKET_RECEIVED) {
		}

		ortp_event_destroy(ev);

		ev = ortp_ev_queue_get(evq_);
	}

	// to get rtp session stats
	const rtp_stats_t *stat = rtp_session_get_stats(rtp_session_);
	stat_.sent = stat->sent;
	stat_.packets = stat->packet_sent;
	//stat_.packets_lost = stat->cum_packet_loss; // 在 RR 中更新

	// 检查是否超时 :)
	if (util_time_now() - time_last_rtcp_ > (double)SINK_TIMEOUT_SECONDS) {
		death_ = true;
	}
}

static void sdes_item_cb(void *user_data, uint32_t csrc, rtcp_sdes_type_t t, const char *content, uint8_t content_len)
{
	return;

	char *buf = (char*)alloca(content_len+1);
	memcpy(buf, content, content_len);
	buf[content_len] = 0;

	if (t == RTCP_SDES_CNAME) {
		log("\t\t CNAME: '%s'\n", buf);
	}
	else if (t == RTCP_SDES_EMAIL) {
		log("\t\t EMAIL: '%s'\n", buf);
	}
	else if (t == RTCP_SDES_NAME) {
		log("\t\t NAME: '%s'\n", buf);
	}
	else if (t == RTCP_SDES_PHONE) {
		log("\t\t PHONE: '%s'\n", buf);
	}
	else if (t == RTCP_SDES_NOTE) {
		log("\t\t NOTE: '%s'\n", buf);
	}
	else if (t == RTCP_SDES_LOC) {
		log("\t\t LOC: '%s'\n", buf);
	}
}

void Sink::process_rtcp(mblk_t *m)
{
	// TODO: 处理收到的 rtcp 包，如果收到 BYE，直接 death_ 即可
	do {
		if (rtcp_is_RR(m)) {
//			log("[Sink]<%d> %s: .....\n", sink_id(), __FUNCTION__);
//			log("\t<RR>\n");
			const rtcp_common_header_t *header = rtcp_get_common_header(m);
			for (int i = 0; i < header->rc; i++) {
				const report_block_t *rb = rtcp_RR_get_report_block(m, i);
				assert(rb);
				float rt = rtp_session_get_round_trip_propagation(rtp_session_);
				unsigned int jitter = report_block_get_interarrival_jitter(rb);
				float lost = 100.0 * report_block_get_fraction_lost(rb)/256.0;

//				log("\t\tssrc=%u: rtt=%f, jitter=%u, lost=%f\n",
//						rb->ssrc, rt, jitter, lost);
				
				stat_.packets_lost += report_block_get_fraction_lost(rb);
				stat_.jitter = jitter;
			}
		}
		else if (rtcp_is_BYE(m)) {
			// FIXME:
			/*
			log("\t<BYE>\n");
			uint32_t ssrc = 0;
			const char *reason = 0;
			int len;
			char *buf = (char*)alloca(len+1);
			memcpy(buf, reason, len);
			buf[len] = 0;
			rtcp_BYE_get_ssrc(m, 0, &ssrc); // FIXME: 仅仅显示第一个
			rtcp_BYE_get_reason(m, &reason, &len);
			log("\t\t<BYE> from %u, to set death flag\n", ssrc);
			log("\t\tbye reason is '%s'\n", buf);
			death_ = 1;
			*/
		}
		else if (rtcp_is_SDES(m)) {
//			log("\t<SDES>\n");
			rtcp_sdes_parse(m, sdes_item_cb, this);
		}
	} while (rtcp_next_packet(m));
}
