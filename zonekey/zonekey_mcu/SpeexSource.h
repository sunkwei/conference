#pragma once
#include "Source.h"
class SpeexSource :
	public Source
{
public:
	SpeexSource(int id, const char *desc);
	~SpeexSource(void);
};

