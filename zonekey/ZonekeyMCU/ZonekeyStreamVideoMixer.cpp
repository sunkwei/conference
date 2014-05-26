#include "ZonekeyStreamVideoMixer.h"
#include <mediastreamer2/msrtp.h>


ZonekeyStreamVideoMixer::ZonekeyStreamVideoMixer(int id, RtpSession *rs)
	: ZonekeyStream(id, rs)
{
	filter_recver_ = ms_filter_new(MS_RTP_RECV_ID);
	ms_filter_call_method(filter_recver_, MS_RTP_RECV_SET_SESSION, rs);

	filter_decoder_ = ms_filter_new(MS_H264_DEC_ID);
	
	filter_sender_ = ms_filter_new(MS_RTP_SEND_ID);
	ms_filter_call_method(filter_sender_, MS_RTP_SEND_SET_SESSION, rs);

	// Á´½Ó recver --> decoder
	ms_filter_link(filter_recver_, 0, filter_decoder_, 0);
}


ZonekeyStreamVideoMixer::~ZonekeyStreamVideoMixer(void)
{
	ms_filter_unlink(filter_recver_, 0, filter_decoder_, 0);

	ms_filter_destroy(filter_sender_);
	ms_filter_destroy(filter_recver_);
	ms_filter_destroy(filter_decoder_);
}
