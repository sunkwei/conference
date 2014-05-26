#pragma once

#include "stream.h"

/** 实现 PCMu 格式，8k, 64kbps, rtp payload type 0
 */
class PCMuStream : public Stream
{
public:
	PCMuStream(int id, const char *desc, bool support_sink = true, bool support_publisher = false);
	~PCMuStream(void);

protected:
	virtual MSFilter *get_source_filter();
	virtual MSFilter *get_sink_filter();

private:
	MSFilter *decoder_, *encoder_;
	MSFilter *resample_in_, *resample_out_;	// 解码后从 8k 转换到 16k，编码前从 16k 转换到 8k
};
