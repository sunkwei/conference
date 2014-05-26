#pragma once

#include <mediastreamer2/mediastream.h>
#include <ortp/ortp.h>
#include "zonekeystream.h"

/** 用于混合的视频流

		对于混合视频流，输出的为 YUV 流，输入为 h264 流
 */
class ZonekeyStreamVideoMixer : public ZonekeyStream
{
public:
	ZonekeyStreamVideoMixer(int id, RtpSession *rs);
	~ZonekeyStreamVideoMixer(void);

	// 返回 rtp recv --> h264 decoder -->
	MSFilter *get_input() { return filter_decoder_; }

	// 返回 --> rtp sender
	MSFilter *get_output() { return filter_sender_; }

	int chid() const { return chid_; }
	void set_chid(int ch) { chid_ = ch; }

private:
	MSFilter *filter_recver_, *filter_decoder_, *filter_sender_;
	int chid_;
};
