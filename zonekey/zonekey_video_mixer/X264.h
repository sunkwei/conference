/** 封装 x264 encoder 的使用，输入为 YUV，输出为 MSQueue，使用 rfc3984 打包
 */

#pragma once

#include <mediastreamer2/mediastream.h>
#include <mediastreamer2/rfc3984.h>
extern "C" {
#	include <x264.h>
}

// 配置
struct X264Cfg
{
	int width, height;		// 输出分辨率
	double fps;				// 帧率
	int kbitrate;			// kbps
	int gop;
};

// encode 输出的状态
struct X264FrameState
{
	int frame_type;	// X264_TYPE_xxx
	int qp;			// last qp
	int64_t pts, dts;

	int nals;
	int bytes;
};

class X264
{
	X264Cfg cfg_;
	x264_t *x264_;
	bool force_key_;
	Rfc3984Context *packer_;
	int64_t next_pts_;

public:
	X264(X264Cfg *cfg);	// 设置输出分辨率
	~X264(void);

	void force_key_frame();	// 强制输出关键帧

	// 压缩 yuv 数据到 queue
	int encode(unsigned char *data[4], int stride[4], MSQueue *queue, X264FrameState *state);

	// 使用 rfc3984 打包, nals 为 x264 压缩后的 nal，rtp 用于保存打包后的数据
	void rfc3984_pack(MSQueue *nals, MSQueue *rtp, uint32_t stamp);

	const X264Cfg *cfg() const { return &cfg_; }
};
