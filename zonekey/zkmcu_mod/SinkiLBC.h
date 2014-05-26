#pragma once
#include "sinkbase.h"

class SinkiLBC : public SinkBase
{
	MSFilter *f_resample_;	// 从 iLBC 的 8k 转换为 AAC 需要的 32k
	MSFilter *f_ilbc_decoder_, *f_aac_encoder_;

public:
	SinkiLBC(const char *mcu_ip, int mcu_rtp_port, int mcu_rtcp_port, void (*data)(void *opaque, double stamp, void *data, int len, bool key), void *opaque);
	~SinkiLBC(void);

private:
	virtual double payload_freq() const { return 32000.0; }	// 输出的 AAC 为 32000
	virtual MSQueue *post_handle(mblk_t *im);
	virtual MSFilter *trans_in() { return f_ilbc_decoder_; }
	virtual MSFilter *trans_out() { return f_aac_encoder_; }
};
