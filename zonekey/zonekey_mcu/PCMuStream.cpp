#include "PCMuStream.h"

PCMuStream::PCMuStream(int id, const char *desc, bool support_sink, bool support_publisher)
	: Stream(id, 0, desc, support_sink, support_publisher)
{
	if (support_sink) 
		encoder_ = ms_filter_new(MS_ULAW_ENC_ID);
	else
		encoder_ = 0;
	decoder_ = ms_filter_new(MS_ULAW_DEC_ID);
	
	int in_sample = 8000, in_ch = 1;
	int out_sample = 16000, out_ch = 1;
	resample_in_ = ms_filter_new(MS_RESAMPLE_ID);
	ms_filter_call_method(resample_in_, MS_FILTER_SET_SAMPLE_RATE, &in_sample);
	ms_filter_call_method(resample_in_, MS_FILTER_SET_NCHANNELS, &in_ch);
	ms_filter_call_method(resample_in_, MS_FILTER_SET_OUTPUT_SAMPLE_RATE, &out_sample);
	ms_filter_call_method(resample_in_, MS_FILTER_SET_OUTPUT_NCHANNELS, &out_ch);

	in_sample = 16000, in_ch = 1;
	out_sample = 8000, out_ch = 1;
	if (encoder_) {
		resample_out_ = ms_filter_new(MS_RESAMPLE_ID);
		ms_filter_call_method(resample_out_, MS_FILTER_SET_SAMPLE_RATE, &in_sample);
		ms_filter_call_method(resample_out_, MS_FILTER_SET_NCHANNELS, &in_ch);
		ms_filter_call_method(resample_out_, MS_FILTER_SET_OUTPUT_SAMPLE_RATE, &out_sample);
		ms_filter_call_method(resample_out_, MS_FILTER_SET_OUTPUT_NCHANNELS, &out_ch);
	}
	else
		resample_out_ = 0;

	if (support_publisher) {
		ms_filter_link(filter_recver_, 0, filter_tee_, 0);
		ms_filter_link(filter_tee_, 0, decoder_, 0);
		ms_filter_link(filter_tee_, 1, filter_publisher_, 0);
	}
	else {
		ms_filter_link(filter_recver_, 0, decoder_, 0);
	}
	ms_filter_link(decoder_, 0, resample_in_, 0);
	if (resample_out_) {
		ms_filter_link(resample_out_, 0, encoder_, 0);
		ms_filter_link(encoder_, 0, filter_sender_, 0);
	}
}

PCMuStream::~PCMuStream(void)
{
	if (encoder_) ms_filter_destroy(encoder_);
	ms_filter_destroy(decoder_);
	ms_filter_destroy(resample_in_);
	if (resample_out_) ms_filter_destroy(resample_out_);
}

MSFilter *PCMuStream::get_sink_filter()
{
	return resample_out_;
}

MSFilter *PCMuStream::get_source_filter()
{
	return resample_in_;
}
