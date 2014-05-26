#include "MemberStream.h"
#include <assert.h>
#include <mediastreamer2/msrtp.h>

MemberStream::MemberStream(int id, MemberStreamType type, int payload)
	: type_(type)
	, id_(id)
{
	filter_sender_ = 0;
	filter_recver_ = 0;
	rtp_session_ = 0;

	if (type_ == SENDRECV)
		rtp_session_ = rtp_session_new(RTP_SESSION_SENDRECV);
	else if (type_ == SENDONLY)
		rtp_session_ = rtp_session_new(RTP_SESSION_SENDONLY);
	else if (type_ == RECVONLY)
		rtp_session_ = rtp_session_new(RTP_SESSION_RECVONLY);

	assert(rtp_session_);

	rtp_session_set_profile(rtp_session_, &av_profile);
	rtp_session_set_payload_type(rtp_session_, payload);

	if (type_ == SENDRECV || type_ == RECVONLY)
		rtp_session_set_local_addr(rtp_session_, get_server_ip(), 0, 0);

	// 设置 jitter buffer
	JBParameters jbp;
	jbp.adaptive = 1;
	jbp.max_packets = 500;	// 高清需要足够大
	jbp.max_size = -1;
	jbp.nom_size = jbp.min_size = 200;	// 200ms
	rtp_session_set_jitter_buffer_params(rtp_session_, &jbp);

	if (type_ != SENDONLY) {
		filter_recver_ = ms_filter_new(MS_RTP_RECV_ID);
		ms_filter_call_method(filter_recver_, MS_RTP_RECV_SET_SESSION, rtp_session_);
	}
	
	if (type != RECVONLY) {
		filter_sender_ = ms_filter_new(MS_RTP_SEND_ID);
		ms_filter_call_method(filter_sender_, MS_RTP_SEND_SET_SESSION, rtp_session_);
	}
}

MemberStream::~MemberStream(void)
{
	if (filter_recver_) ms_filter_destroy(filter_recver_);
	if (filter_sender_) ms_filter_destroy(filter_sender_);
	rtp_session_destroy(rtp_session_);
}
