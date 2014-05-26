#pragma once
#include "Sink.h"
class H264Sink :
	public Sink
{
public:
	H264Sink(int id, const char *desc, const char *who);
	~H264Sink(void);
};

