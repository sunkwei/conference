#include "ZonekeySink.h"

ZonekeySink::ZonekeySink(int id, RtpSession *rs)
	: id_(id)
	, rtp_session_(rs)
{
}

ZonekeySink::~ZonekeySink(void)
{
}
