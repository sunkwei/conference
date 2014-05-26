#pragma once
#include "videorecver.h"

/**  rtp recver --> h264 decoder --> yuv sink 
 */
class VideoRecverYUV :	public VideoRecver
{
public:
	VideoRecverYUV(void);
	~VideoRecverYUV(void);
};

