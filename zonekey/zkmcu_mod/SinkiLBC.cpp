#include "StdAfx.h"
#include "SinkiLBC.h"

/** 接收为 iLBC，输出需要转换为 AAC */
SinkiLBC::SinkiLBC(const char *mcu_ip, int mcu_rtp_port, int mcu_rtcp_port, void (*data)(void *opaque, double stamp, void *data, int len, bool key), void *opaque)
	: SinkBase(102, mcu_ip, mcu_rtp_port, mcu_rtcp_port, data, opaque)
{
	f_resample_ = ms_filter_new(MS_RESAMPLE_ID);
	int in_sample = 8000, in_ch = 1;
	int out_sample = 32000, out_ch = 1;
	ms_filter_call_method(f_resample_, MS_FILTER_SET_SAMPLE_RATE, &in_sample);
	ms_filter_call_method(f_resample_, MS_FILTER_SET_NCHANNELS, &in_ch);
	ms_filter_call_method(f_resample_, MS_FILTER_SET_OUTPUT_SAMPLE_RATE, &out_sample);
	ms_filter_call_method(f_resample_, MS_FILTER_SET_OUTPUT_NCHANNELS, &out_ch);

	f_ilbc_decoder_ = ms_filter_new_from_name("MSIlbcDec");
	f_aac_encoder_ = ms_filter_new_from_name("ZonekeyAACEnc");

	ms_filter_link(f_ilbc_decoder_, 0, f_resample_, 0);
	ms_filter_link(f_resample_, 0, f_aac_encoder_, 0);
}

SinkiLBC::~SinkiLBC(void)
{
	ms_filter_destroy(f_resample_);
	ms_filter_destroy(f_ilbc_decoder_);
	ms_filter_destroy(f_aac_encoder_);
}

MSQueue *SinkiLBC::post_handle(mblk_t *im)
{
	ms_queue_put(&queue_, im);
	return &queue_;
}
