#pragma once

#include "Stream.h"

class H264Stream : public Stream
{
public:
	H264Stream(int id, const char *desc, bool support_sink = true, bool support_publisher = false);
	~H264Stream();

protected:
	virtual MSFilter *get_source_filter();
	virtual MSFilter *get_sink_filter();

private:
	MSFilter *decoder_;
};
