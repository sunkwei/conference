/** 画布，相当于一个 yuv 图像，支持背景，大小等属性
 */
#pragma once

#include <stdint.h>
extern "C" {
#	include <libavcodec/avcodec.h>
}

class Canvas
{
	int width_, height_;	// 画布大小
	AVPicture pic_;			// 用于存储画布

public:
	Canvas(int width, int height);
	virtual ~Canvas(void);

	int width() const { return width_; }
	int height() const { return height_; }

	void clear();	// 清空画布

	// 画 YUV 到指定位置
	int draw_yuv(unsigned char *data[4], int stride[4],
		int x, int y, int width, int height, int alpha);

	AVPicture get_pic() const { return pic_; }	// 这个在 mixer thread 中调用，一般不需要加锁
};
