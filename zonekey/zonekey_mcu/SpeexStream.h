#pragma once

#include "Stream.h"

class SpeexStream : public Stream
{
public:
	SpeexStream(int id, const char *desc, bool support_sink = true, bool support_publisher = false);
	~SpeexStream();

protected:
	MSFilter *get_source_filter();
	MSFilter *get_sink_filter();

private:
	MSFilter *decoder_, *encoder_;
};

