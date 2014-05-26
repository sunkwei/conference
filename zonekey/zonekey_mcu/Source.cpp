#include "Source.h"
#include <assert.h>
#include <mediastreamer2/msrtp.h>
#include "main_def.h"

Source::Source(int id, int pt, const char *desc)
	: id_(id)
	, pt_(pt)
{
	if (desc) desc_ = desc;
	memset(&stat_, 0, sizeof(stat_));

	death_ = false;
	time_last_rtcp_ = util_time_now();

	myip_ = util_get_myip();
	rtp_sess_ = rtp_session_new(RTP_SESSION_RECVONLY);
	if (pt == 100) {
		rtp_session_set_rtp_socket_recv_buffer_size(rtp_sess_, 1024*1024);	// for HD
		rtp_session_set_rtp_socket_send_buffer_size(rtp_sess_, 1024*1024);
	}
	else {
		rtp_session_set_rtp_socket_recv_buffer_size(rtp_sess_, 32*1024);
		rtp_session_set_rtp_socket_send_buffer_size(rtp_sess_, 32*1024);
	}

	rtp_session_set_payload_type(rtp_sess_, pt);
	//rtp_session_set_local_addr(rtp_sess_, myip_.c_str(), 0, 0);	// 随机
	rtp_session_set_local_addr(rtp_sess_, util_get_myip_real(), 0, 0); // 需要支持内网，外网的情况

	// 禁用 jitterbuffer
	rtp_session_enable_jitter_buffer(rtp_sess_, 0);
	rtp_session_enable_adaptive_jitter_compensation(rtp_sess_, 0);
	
	filter_ = ms_filter_new(MS_RTP_RECV_ID);
	ms_filter_call_method(filter_, MS_RTP_RECV_SET_SESSION, rtp_sess_);

	evq_ = ortp_ev_queue_new();
	rtp_session_register_event_queue(rtp_sess_, evq_);
}

Source::~Source(void)
{
	ms_filter_destroy(filter_);
	rtp_session_destroy(rtp_sess_);
	ortp_ev_queue_destroy(evq_);
}

void Source::run()
{
	if (death_) return;

	/** call from Conference threading, 
	    for RTCP process ....
	 */

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

	const rtp_stats_t *stat = rtp_session_get_stats(rtp_sess_);
	stat_.recv = stat->recv;
	stat_.hw_recv = stat->hw_recv;
	stat_.recv_packets = stat->packet_recv;
	stat_.lost_packets = stat->cum_packet_loss;

	if (util_time_now() - time_last_rtcp_ > (double)SOURCE_TIMEOUT_SECONDS)
		death_ = true;
}

void Source::process_rtcp(mblk_t *m)
{
	do {
		if (rtcp_is_SR(m))
			on_rtcp_sr(m);
		else if (rtcp_is_RR(m))
			on_rtcp_rr(m);
		else if (rtcp_is_SDES(m))
			on_rtcp_sdes(m);
		else if (rtcp_is_BYE(m))
			on_rtcp_bye(m);
		else if (rtcp_is_APP(m))
			on_rtcp_app(m);
	} while (rtcp_next_packet(m));
}

bool Source::death() const
{
	return death_;
}

const char *Source::desc()
{
	if (desc_.empty()) {
		return "no desc";
	}
	return desc_.c_str();
}

int Source::source_id()
{
	return id_;
}

int Source::payload_type()
{
	return pt_;
}

const char *Source::get_ip()
{
	return myip_.c_str();
}

int Source::get_rtcp_port()
{
	int sock = rtp_session_get_rtcp_socket(rtp_sess_);
	sockaddr_in sin;
	memset(&sin, 0, sizeof(sin));
	socklen_t len = sizeof(sin);
	getsockname(sock, (sockaddr*)&sin, &len);
	return ntohs(sin.sin_port);
}

int Source::get_rtp_port()
{
	int sock = rtp_session_get_rtp_socket(rtp_sess_);
	sockaddr_in sin;
	socklen_t len = sizeof(sin);
	getsockname(sock, (sockaddr*)&sin, &len);
	return ntohs(sin.sin_port);

	return rtp_session_get_local_port(rtp_sess_);
}

MSFilter *Source::get_output()
{
	return filter_;
}

SourceStat Source::stat()
{
	return stat_;
}

void Source::on_rtcp_rr(mblk_t *m)
{
	log("[Source] %s: \n", __FUNCTION__);
	log("\t?????\n");
}

void Source::on_rtcp_sr(mblk_t *m)
{
	log("[Source]\n\t<SR>\n");

	// to read report
	const rtcp_common_header *header = rtcp_get_common_header(m);
	for (int i = 0;i < header->rc; i++) {
		const report_block_t *rb = rtcp_SR_get_report_block(m, i);
		assert(rb);
		float rt = rtp_session_get_round_trip_propagation(rtp_sess_);
		unsigned int jitter = report_block_get_interarrival_jitter(rb);
		float lost = 100.0 * report_block_get_fraction_lost(rb)/256.0;

		log("\t\tssrc=%u: rtt=%f, jitter=%u, lost=%f\n",
				rb->ssrc, rt, jitter, lost);
	}
}

void Source::sdes_item_cb(void *user_data, uint32_t csrc, rtcp_sdes_type_t t, const char *content, uint8_t content_len)
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

void Source::on_rtcp_sdes(mblk_t *m)
{
	log("[Source]\n\t<SDES>\n", __FUNCTION__);
	rtcp_sdes_parse(m, sdes_item_cb, this);
}

void Source::on_rtcp_app(mblk_t *m)
{
	log("[Source] %s: \n", __FUNCTION__);
}

void Source::on_rtcp_bye(mblk_t *m)
{
	// FIXME:
	return;

	log("\t<BYE>\n");
	uint32_t ssrc = 0;
	const char *reason = 0;
	int len = 1024;
	char *buf = (char*)alloca(len+1);
	memcpy(buf, reason, len);
	buf[len] = 0;
	rtcp_BYE_get_ssrc(m, 0, &ssrc); // FIXME: 仅仅显示第一个
	rtcp_BYE_get_reason(m, &reason, &len);
	log("\t\t<BYE> from %u, to set death flag\n", ssrc);
	log("\t\tbye reason is '%s'\n", buf);
	death_ = 1;
}
