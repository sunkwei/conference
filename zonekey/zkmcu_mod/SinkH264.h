#pragma once
#include "sinkbase.h"
#include <mediastreamer2/rfc3984.h>

class SinkH264 : public SinkBase
{
	Rfc3984Context *packer_;

public:
	SinkH264(const char *mcu_ip, int mcu_rtp_port, int mcu_rtcp_port, void (*data)(void *opaque, double stamp, void *data, int len, bool key), void *opaque);
	~SinkH264(void);

private:
	virtual double payload_freq() const { return 90000.0; }
	virtual MSQueue *post_handle(mblk_t *im);

	virtual int size_for(int index, mblk_t *m);
	virtual void save_for(int index, mblk_t *m, unsigned char *buf);
	virtual bool is_key(int index, mblk_t *m);
};
