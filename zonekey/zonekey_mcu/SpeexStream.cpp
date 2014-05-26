#include "SpeexStream.h"

SpeexStream::SpeexStream(int id, const char *desc, bool support_sink, bool support_publisher)
	: Stream(id, 110, desc, support_sink, support_publisher)
{
	decoder_ = ms_filter_new(MS_SPEEX_DEC_ID);
	if (this->support_sink()) 
		encoder_ = ms_filter_new(MS_SPEEX_ENC_ID);
	else
		encoder_ = 0;

	// always 1ch, 16bits, 16k
	int rate = 16000, ch = 1;
	ms_filter_call_method(encoder_, MS_FILTER_SET_SAMPLE_RATE, &rate);
	ms_filter_call_method(decoder_, MS_FILTER_SET_SAMPLE_RATE, &rate);
	ms_filter_call_method(encoder_, MS_FILTER_SET_NCHANNELS, &ch);
	ms_filter_call_method(decoder_, MS_FILTER_SET_NCHANNELS, &ch);

	if (support_publisher) {
		ms_filter_link(filter_recver_, 0, filter_tee_, 0);
		ms_filter_link(filter_tee_, 0, decoder_, 0);
		ms_filter_link(filter_tee_, 1, filter_publisher_, 0);
	}
	else {
		ms_filter_link(filter_recver_, 0, decoder_, 0);
	}

	if (encoder_)
		ms_filter_link(encoder_, 0, filter_sender_, 0);
}

SpeexStream::~SpeexStream()
{
	ms_filter_destroy(decoder_);
	if (encoder_) ms_filter_destroy(encoder_);
}

MSFilter *SpeexStream::get_source_filter()
{
	return decoder_;
}

MSFilter *SpeexStream::get_sink_filter()
{
	return encoder_;
}
