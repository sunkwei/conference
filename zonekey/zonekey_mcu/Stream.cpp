#include <stdio.h>
#include <stdlib.h>
#include "Stream.h"
#include <ortp/rtp.h>
#include <mediastreamer2/mediastream.h>
#include <mediastreamer2/msrtp.h>
#include <assert.h>
#include "util.h"
#include "log.h"
#include "main_def.h"
#include "Sink.h"
#include <mediastreamer2/mstee.h>
#include <mediastreamer2/zk.publisher.h>
#include "iLBC_Stream.h"

#if USER_TRANSPORT

static ortp_socket_t _getsocket(struct _RtpTransport *t);
static int _sendto(struct _RtpTransport *t, mblk_t *msg , int flags, const struct sockaddr *to, socklen_t tolen);
static int _recvfrom(struct _RtpTransport *t, mblk_t *msg, int flags, struct sockaddr *from, socklen_t *fromlen);
static void _close(struct _RtpTransport *transport, void *userData);

static int _set_non_blocking_socket (ortp_socket_t sock)
{
#if	!defined(_WIN32) && !defined(_WIN32_WCE)
	return fcntl (sock, F_SETFL, O_NONBLOCK);
#else
	unsigned long nonBlock = 1;
	return ioctlsocket(sock, FIONBIO , &nonBlock);
#endif
}

struct ServerTransport : public _RtpTransport
{
	int fd_;
	sockaddr_in remote_addr_;	//
	int payload_;

public:
	ServerTransport(int bufsize, int payload)
	{
		payload_ = payload;
		t_getsocket = _getsocket;
		t_sendto = _sendto;
		t_recvfrom = _recvfrom;
		t_close = _close;

		// FIXME: 目前仅仅考虑支持 ipv4 udp
		fd_ = socket(AF_INET, SOCK_DGRAM, 0);

		sockaddr_in local;
		local.sin_family = AF_INET;
		local.sin_addr.s_addr = inet_addr(util_get_myip_real());
		local.sin_port = 0;
		bind(fd_, (sockaddr*)&local, sizeof(local));

		setsockopt(fd_, SOL_SOCKET, SO_RCVBUF, (char*)&bufsize, sizeof(bufsize));
		setsockopt(fd_, SOL_SOCKET, SO_SNDBUF, (char*)&bufsize, sizeof(bufsize));

		_set_non_blocking_socket(fd_);

		/** FIXME: 在 recvfrom() 中设置，如果恰好有干扰数据包呢？ :(
		 */
		remote_addr_.sin_family = 0;
	}

	void close(void *data)
	{
#ifdef WIN32
		closesocket(fd_);
#else
		::close(fd_);
#endif // 
	}

	int sendto(mblk_t *m, int flags, const struct sockaddr *to, socklen_t tolen)
	{
		if (remote_addr_.sin_family == 0)
			return 0;
		
		if (m->b_cont) msgpullup(m, -1);
		int rc = ::sendto(fd_, (char*)m->b_rptr, (int)(m->b_wptr - m->b_rptr), 0, (sockaddr*)&remote_addr_, sizeof(remote_addr_));
		return rc;
	}

	int recvfrom(mblk_t *m, int flags, struct sockaddr *from, socklen_t *fromlen)
	{
		int bufsz = (int) (m->b_datap->db_lim - m->b_datap->db_base);
		int rc = ::recvfrom(fd_, (char *)m->b_wptr, bufsz, flags, from, fromlen);
		if (rc > 0) {
			/** FIXME: 此时收到了数据包，应该更严格的检查！！！！

					检查1：必须是 rtp/rtcp 数据
			 */
			unsigned char *data = (unsigned char*)m->b_wptr;
			if (remote_addr_.sin_family == 0) {
				if (bufsz > 24 && (*data >> 6 == 2)) {
					sockaddr_in *sin = (sockaddr_in*)from;
					log("[Stream] %s: PAYLOAD=%d: en, the remote is %s:%d.................\n", __FUNCTION__, payload_, inet_ntoa(sin->sin_addr), ntohs(sin->sin_port));
					remote_addr_ = *(sockaddr_in*)from;
				}
				else 
					return -1;
			}
			
			if (bufsz > 24 && (*data >> 6) == 2) {
				return rc;
			}
			else {
				return -1;
			}
		}

		return rc;
	}

	void flush_sock(int fd)
	{
		while (1) {
			char c;
			sockaddr_in sin;
			socklen_t len = sizeof(sin);
			int rc = ::recvfrom(fd, &c, 1, 0, (sockaddr*)&sin, &len);
			if (rc < 0)
				return;
		}
	}
};

static ortp_socket_t _getsocket(struct _RtpTransport *t)
{
	ServerTransport *st = (ServerTransport*)t;
	return st->fd_;
}

static int _sendto(struct _RtpTransport *t, mblk_t *msg , int flags, const struct sockaddr *to, socklen_t tolen)
{
	ServerTransport *st = (ServerTransport*)t;
	return st->sendto(msg, flags, to, tolen);
}

static int _recvfrom(struct _RtpTransport *t, mblk_t *msg, int flags, struct sockaddr *from, socklen_t *fromlen)
{
	ServerTransport *st = (ServerTransport*)t;
	return st->recvfrom(msg, flags, from, fromlen);
}

static void _close(struct _RtpTransport *t, void *userData)
{
	ServerTransport *st = (ServerTransport*)t;
	st->close(userData);
}
#endif

Stream::Stream(int id, int payload, const char *desc, bool support_sink, bool support_publisher)
	: id_(id)
	, payload_(payload)
	, desc_(desc)
{
	memset(&stat_, 0, sizeof(stat_));

	death_ = false;
	last_check_stamp_ = util_time_now();
	packet_recved_ = 0;

	/** XXX: 对于 Server 端的 Stream，一般情况下，是无法 直接 rtp_session_set_remote_addr_and_port() 的，往往需要收到 remote 的数据之后，才能知道
			其真正地址（一般可能是 NAT 的）

			因此，需要实现 RtpTransport ....

			如果 !support_sink，则不需要时 trans
	 */
	int bufsize = 32*1024;
	if (payload == 100)
		bufsize = 1024*1024;

#if USER_TRANSPORT
	if (support_sink) {
		trans_rtp_ = new ServerTransport(bufsize, payload);
		trans_rtcp_ = new ServerTransport(bufsize, 0 - payload);
	}
	else {
		trans_rtp_ = 0;
		trans_rtcp_ = 0;
	}
#endif

	if (support_sink)
		rtp_sess_ = rtp_session_new(RTP_SESSION_SENDRECV);
	else
		rtp_sess_ = rtp_session_new(RTP_SESSION_RECVONLY);

#if USER_TRANSPORT
	if (support_sink)
		rtp_session_set_transports(rtp_sess_, trans_rtp_, trans_rtcp_);
	else {
		rtp_session_set_rtp_socket_recv_buffer_size(rtp_sess_, bufsize);
		rtp_session_set_rtp_socket_send_buffer_size(rtp_sess_, bufsize);
		rtp_session_set_local_addr(rtp_sess_, util_get_myip_real(), 0, 0);
	}
#else
	rtp_session_set_rtp_socket_recv_buffer_size(rtp_sess_, bufsize);
	rtp_session_set_rtp_socket_send_buffer_size(rtp_sess_, bufsize);
	rtp_session_set_local_addr(rtp_sess_, util_get_myip_real(), 0, 0);
	rtp_session_set_symmetric_rtp(rtp_sess_, 1);
#endif 

	rtp_session_set_payload_type(rtp_sess_, payload);

	// 视频 禁用 jitter buffer
	if (payload_ == 100) {
		rtp_session_enable_adaptive_jitter_compensation(rtp_sess_, 0);
		rtp_session_enable_jitter_buffer(rtp_sess_, 0);
	}
	else {
		// 音频需要参与 mixer，所以还是需要 jitter buffer的，使用80看看效果，可能需要作为一个配置项，或者根据 jitter 决定
		JBParameters jb;
		jb.adaptive = 1;
		jb.max_packets = 100;
		jb.min_size = jb.nom_size = 80;
		jb.max_size = -1;
		rtp_session_set_jitter_buffer_params(rtp_sess_, &jb);
		rtp_session_enable_jitter_buffer(rtp_sess_, 1);
	}

	evq_ = ortp_ev_queue_new();
	rtp_session_register_event_queue(rtp_sess_, evq_);

	int sock = rtp_session_get_rtp_socket(rtp_sess_);
	sockaddr_in name;
	socklen_t len = sizeof(name);
	getsockname(sock, (sockaddr*)&name, &len);
	rtp_port_ = ntohs(name.sin_port);

	sock = rtp_session_get_rtcp_socket(rtp_sess_);
	getsockname(sock, (sockaddr*)&name, &len);
	rtcp_port_ = ntohs(name.sin_port);

	filter_recver_ = ms_filter_new(MS_RTP_RECV_ID);
	ms_filter_call_method(filter_recver_, MS_RTP_RECV_SET_SESSION, rtp_sess_);

	if (support_sink) {
		filter_sender_ = ms_filter_new(MS_RTP_SEND_ID);
		ms_filter_call_method(filter_sender_, MS_RTP_SEND_SET_SESSION, rtp_sess_);
	}
	else
		filter_sender_ = 0;

	if (!support_publisher) {
		filter_publisher_ = 0;
		filter_tee_ = 0;
	}
	else {
		filter_publisher_ = ms_filter_new_from_name("ZonekeyPublisher");
		filter_tee_ = ms_filter_new(MS_TEE_ID);
		int mode = 1; // 使用 copymsg
		ms_filter_call_method(filter_tee_, MS_TEE_SET_MODE, &mode);
	}
}

Stream::~Stream()
{
	if (filter_tee_) ms_filter_destroy(filter_tee_);
	if (filter_publisher_) ms_filter_destroy(filter_publisher_);

	if (filter_sender_)
		ms_filter_destroy(filter_sender_);
	ms_filter_destroy(filter_recver_);
	rtp_session_destroy(rtp_sess_);
	ortp_ev_queue_destroy(evq_);

#if USER_TRANSPORT
	delete trans_rtp_;
	delete trans_rtcp_;
#endif
}

int Stream::add_sink(Sink *sink)
{
	assert(support_publisher());

	ost::MutexLock al(cs_sinks_);
	sinks_.push_back(sink);
	fprintf(stderr, "[mcu]: %s: add sink of %s\n", __FUNCTION__, sink->desc());
	ms_filter_call_method(filter_publisher_, ZONEKEY_METHOD_PUBLISHER_ADD_REMOTE, sink->get_rtp_session());
	return sinks_.size();
}

int Stream::del_sink(Sink *sink)
{
	if (!support_publisher()) return -1;
	ost::MutexLock al(cs_sinks_);
	fprintf(stderr, "[mcu]: %s: remove sink of %s\n", __FUNCTION__, sink->desc());
	for (std::vector<Sink*>::iterator it = sinks_.begin(); it != sinks_.end(); ++it) {
		if (*it == sink) {
			ms_filter_call_method(filter_publisher_, ZONEKEY_METHOD_PUBLISHER_DEL_REMOTE, sink->get_rtp_session());
			sinks_.erase(it);
			break;
		}
	}

	return sinks_.size();
}

void Stream::run()
{
	/** 在 Conference Threading 中调用，获取 rtcp，解析
	 */
	if (death_) return;

	assert(evq_ && rtp_sess_);
	OrtpEvent *ev = ortp_ev_queue_get(evq_);
	while (ev) {
		OrtpEventType type = ortp_event_get_type(ev);
		if (type == ORTP_EVENT_RTCP_PACKET_RECEIVED) {
			process_rtcp(ortp_event_get_data(ev)->packet);
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
	stat_.sent = stat->sent;
	stat_.packet_recv = stat->packet_recv;
	stat_.packet_sent = stat->packet_sent;
	stat_.packet_lost_recv = stat->cum_packet_loss;

	// 对于 Stream，可以根据 stat 中 packet_recv 是否连续变化，判断是否已经长时间没收到包了
	death_ = is_timeout(util_time_now(), stat);
}

bool Stream::is_timeout(double now, const rtp_stats_t *stat)
{
	// 如果连续 STREAM_TIMEOUT_SECONDS 没有收到任何 rtp 包，则认为超时
	if (now - last_check_stamp_ > (double)STREAM_TIMEOUT_SECONDS) {
		if (packet_recved_ == stat->packet_recv) {
			return true;
		}

		last_check_stamp_ = now;
		packet_recved_ = stat->packet_recv;
	}

	return false;
}

void Stream::process_rtcp(mblk_t *m)
{
	do {
		if (rtcp_is_SR(m))
			on_rtcp_sr(m);
		else if (rtcp_is_RR(m))
			on_rtcp_rr(m);
		else if (rtcp_is_SDES(m))
			on_rtcp_sdes(m);
		else if (rtcp_is_APP(m))
			on_rtcp_app(m);
		else if (rtcp_is_BYE(m))
			on_rtcp_bye(m);
	} while (rtcp_next_packet(m));
}

void Stream::on_rtcp_sr(mblk_t *m)
{
	const rtcp_common_header *header = rtcp_get_common_header(m);
	for (int i = 0; i < header->rc; i++) {
		const report_block_t *rb = rtcp_SR_get_report_block(m, i);
		assert(rb);

		float rt = rtp_session_get_round_trip_propagation(rtp_sess_);
		unsigned int jitter = report_block_get_interarrival_jitter(rb);
		float lost = 100.0 * report_block_get_fraction_lost(rb)/256.0;
		stat_.packet_lost_sent += report_block_get_fraction_lost(rb);
		stat_.jitter = report_block_get_interarrival_jitter(rb);
	}
}

void Stream::on_rtcp_rr(mblk_t *m)
{
	const rtcp_common_header *header = rtcp_get_common_header(m);
	for (int i = 0; i < header->rc; i++) {
		const report_block_t *rb = rtcp_RR_get_report_block(m, i);
		assert(rb);

		float rt = rtp_session_get_round_trip_propagation(rtp_sess_);
		unsigned int jitter = report_block_get_interarrival_jitter(rb);
		float lost = 100.0 * report_block_get_fraction_lost(rb)/256.0;

		stat_.packet_lost_sent += report_block_get_fraction_lost(rb);
		stat_.jitter = report_block_get_interarrival_jitter(rb);
	}
}

void Stream::on_rtcp_app(mblk_t *m)
{
}

void Stream::on_rtcp_bye(mblk_t *m)
{
	return; // FIXME:

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

void Stream::on_rtcp_sdes(mblk_t *m)
{
	//log("\t<SDES\n");
	rtcp_sdes_parse(m, sdes_item_cb, this);
}
