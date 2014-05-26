#pragma once

#include "Sink.h"

class iLBCSink : public Sink
{
public:
	iLBCSink(int id, const char *desc, const char *who);
	~iLBCSink();
};
