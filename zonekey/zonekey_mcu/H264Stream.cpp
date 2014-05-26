#include "H264Stream.h"
#include <ortp/rtp.h>

H264Stream::H264Stream(int id, const char *desc, bool support_sink, bool support_publisher)
	: Stream(id, 100, desc, support_sink, support_publisher)
{
	decoder_ = ms_filter_new(MS_H264_DEC_ID);
	if (support_publisher) {
		// 需要支持 publisher
		ms_filter_link(filter_recver_, 0, filter_tee_, 0);
		ms_filter_link(filter_tee_, 0, decoder_, 0);
		ms_filter_link(filter_tee_, 1, filter_publisher_, 0);
	}
	else {
		ms_filter_link(filter_recver_, 0, decoder_, 0);
	}
}

H264Stream::~H264Stream()
{
	ms_filter_destroy(decoder_);
}

MSFilter *H264Stream::get_source_filter()
{
	return decoder_;
}

MSFilter *H264Stream::get_sink_filter()
{
	return filter_sender_;
}
