/** 一路流的描述：
 */		

#pragma once

#include <ortp/ortp.h>
#include <string>

class ZonekeyStream
{
public:
	ZonekeyStream(int id, RtpSession *rs);
	virtual ~ZonekeyStream(void);

	int id() const { return id_; }
	RtpSession *rtp_session() { return rtp_session_; }
	const char *server_ip() const { return server_ip_.c_str(); }
	int server_port() const { return server_port_; }

protected:
	int id_;
	RtpSession *rtp_session_;
	std::string server_ip_;
	int server_port_;
};
