#pragma once

#include <ortp/ortp.h>

/** 对应一个纯粹的接收者，与 ZonekeyStream 相较
 */
class ZonekeySink
{
public:
	ZonekeySink(int id, RtpSession *rs);
	virtual ~ZonekeySink(void);

	int id() const { return id_; }
	RtpSession *rtp_session() { return rtp_session_; }

private:
	int id_;
	RtpSession *rtp_session_;
};
