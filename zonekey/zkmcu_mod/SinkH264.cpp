#include "StdAfx.h"
#include "SinkH264.h"
#include <assert.h>

SinkH264::SinkH264(const char *mcu_ip, int mcu_rtp_port, int mcu_rtcp_port, void (*data)(void *opaque, double stamp, void *data, int len, bool key), void *opaque)
	: SinkBase(100, mcu_ip, mcu_rtp_port, mcu_rtcp_port, data, opaque)
{
	packer_ = rfc3984_new();
}


SinkH264::~SinkH264(void)
{
	rfc3984_destroy(packer_);
}

MSQueue* SinkH264::post_handle(mblk_t *im)
{
	// 使用 rfc3984 解包 ...
	rfc3984_unpack(packer_, im, &queue_);

	// 解包后，还需要打成 annex b 格式，就是 00 00 00 01 xx 的摸样
	return &queue_;
}

int SinkH264::size_for(int index, mblk_t *m)
{
	// 对于 h264 的nals，index == 0时，返回 +4, 填充 00 00 00 01
	// 对于其他的返回的，返回  +3，用于填充 00 00 01
	if (index == 0)
		return m->b_wptr - m->b_rptr + 4;
	else
		return m->b_wptr - m->b_rptr + 3;
}

void SinkH264::save_for(int index, mblk_t *m, unsigned char *buf)
{
	if (index == 0) {
		buf[0] = buf[1] = buf[2] = 0;
		buf[3] = 1;
		buf += 4;
	}
	else {
		buf[0] = buf[1] = 0;
		buf[2] = 1;
		buf += 3;
	}
	memcpy(buf, m->b_rptr, m->b_wptr - m->b_rptr);
}

bool SinkH264::is_key(int index, mblk_t *m)
{
	// 如果第一个是 sps，则认为是 key
	assert(index == 0);
	return m->b_rptr[0] == 0x67;
}
