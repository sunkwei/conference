#include "ZonekeyStream.h"
#include "util.h"

ZonekeyStream::ZonekeyStream(int id, RtpSession *rs)
	: id_(id)
	, rtp_session_(rs)
{
	server_ip_ = util_get_myip();
	server_port_ = rtp_session_get_local_port(rs);
}

ZonekeyStream::~ZonekeyStream(void)
{
}
