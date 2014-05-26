#include "StdAfx.h"
#include "SinkBase.h"
#include <mediastreamer2/zk.void.h>
#include <malloc.h>

SinkBase::SinkBase(int payload, const char *mcu_ip, int mcu_rtp_port, int mcu_rtcp_port, void (*data)(void *opaque, double stamp, void *data, int len, bool key), void *opaque)
	: cb_data_(data)
	, opaque_(opaque)
	, mcu_ip_(mcu_ip)
	, mcu_rtp_port_(mcu_rtp_port)
	, mcu_rtcp_port_(mcu_rtcp_port)
	, payload_(payload)
{
	rtp_ = rtp_session_new(RTP_SESSION_RECVONLY);
	if (payload == 100) {
		rtp_session_set_rtp_socket_recv_buffer_size(rtp_, 1024*1024);
	}
	rtp_session_set_remote_addr_and_port(rtp_, mcu_ip, mcu_rtp_port, mcu_rtcp_port);
	rtp_session_set_payload_type(rtp_, payload);
	rtp_session_enable_jitter_buffer(rtp_, false);

	f_recv_ = ms_filter_new(MS_RTP_RECV_ID);
	ms_filter_call_method(f_recv_, MS_RTP_RECV_SET_SESSION, rtp_);

	f_void_ = ms_filter_new_from_name("ZonekeyVoidSink");
	ZonekeyVoidSinkCallback cbs = { this, cb_data };
	ms_filter_call_method(f_void_, ZONEKEY_METHOD_VOID_SINK_SET_CALLBACK, &cbs);

	ticker_ = ms_ticker_new();

	ms_queue_init(&queue_);
}

SinkBase::~SinkBase(void)
{
	ms_ticker_destroy(ticker_);
	ms_filter_destroy(f_void_);
	ms_filter_destroy(f_recv_);
	rtp_session_destroy(rtp_);
}

int SinkBase::Start()
{
	first_frame_ = true;
	next_stamp_ = 0.0;

	// link filters
	MSFilter *trin = trans_in();
	MSFilter *trout = trans_out();
	if (trin && trout) {
		ms_filter_link(f_recv_, 0, trin, 0);
		ms_filter_link(trout, 0, f_void_, 0);
	}
	else {
		ms_filter_link(f_recv_, 0, f_void_, 0);
	}

	ms_ticker_attach(ticker_, f_recv_);
	return 0;
}

int SinkBase::Stop()
{
	ms_ticker_detach(ticker_, f_recv_);
	return 0;
}

void SinkBase::cb_data(mblk_t *m)
{
	uint32_t st = mblk_get_timestamp_info(m);

	mblk_t *fm = dupmsg(m);
	mblk_meta_copy(m, fm);
	MSQueue *queue = post_handle(fm);	// 此处 dupmsg(m) 将不会导致 m 被释放
	bool first = true;

	unsigned char *obuf = 0;
	size_t olen = 0;
	int index = 0;
	size_t off = 0;
	bool key = false;

	while (mblk_t *om = ms_queue_get(queue)) {

		int dlen = size_for(index, om);
		obuf = (unsigned char*)realloc(obuf, off + dlen);
		save_for(index, om, obuf + off);

		if (index == 0)
			key = is_key(index, om);

		index++;
		off += dlen;

		// 处理时间戳回绕
		if (first_frame_) {
			first_frame_ = false;
		}
		else {
			uint32_t delta = st - last_stamp_;

			// 检查，是否乱序，乱序包直接扔掉！
			if (delta > 0x80000000) {
				fprintf(stderr, "??? maybe timestamp confusioned!, curr=%u, last=%u\n", st, last_stamp_);
				return;
			}

			next_stamp_ += delta / payload_freq();
		}

		last_stamp_ = st;

		freemsg(om);
	}

	if (cb_data_ && obuf) {
		cb_data_(opaque_, next_stamp_, obuf, off, key);
	}

	if (obuf) free(obuf);
}

static void active_sock(int fd, const char *ip, int port)
{
	sockaddr_in sin;
	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);
	sin.sin_addr.s_addr = inet_addr(ip);

	::sendto(fd, "abcd", 4, 0, (sockaddr*)&sin, sizeof(sin));
}


void SinkBase::RunOnce()
{
	// FIXME: 不处理 rtcp 数据了

	// FIXME: 此时简单的发送 rtp, rtcp 激活包，更好的方式应该是受到 rtp, rtcp 包之后，就不再发送了 
	active_sock(rtp_session_get_rtp_socket(rtp_), mcu_ip_.c_str(), mcu_rtp_port_);
	active_sock(rtp_session_get_rtcp_socket(rtp_), mcu_ip_.c_str(), mcu_rtcp_port_);
}
