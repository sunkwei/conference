/** 一路音频 stream ....
	
		提供声音，并且接受混音后的声音数据


 */

#pragma once

#include <ortp/ortp.h>
#include <mediastreamer2/mediastream.h>

#include "zonekeystream.h"
class ZonekeyStreamAudio : public ZonekeyStream
{
public:
	ZonekeyStreamAudio(int id, RtpSession *rs);
	virtual ~ZonekeyStreamAudio(void);

	MSFilter *get_input() { return filter_decoder_; }

private:
	MSFilter *filter_decoder_;
};
