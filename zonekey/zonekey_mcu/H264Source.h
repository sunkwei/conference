#pragma once

#include "Source.h"

class H264Source : public Source
{
public:
	H264Source(int id, const char *desc);
	~H264Source(void);
};
