#pragma once
#include "Sink.h"
class SpeexSink : public Sink
{
public:
	SpeexSink(int id, const char *desc, const char *who);
	~SpeexSink(void);
};

