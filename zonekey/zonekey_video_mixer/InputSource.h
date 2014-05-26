/** 描述一路输入，每录输入对应一个 Member
 */

#pragma once

#include <mediastreamer2/mediastream.h>
#include <cc++/thread.h>
extern "C" {
#	include <libswscale/swscale.h>
#	include <libavcodec/avcodec.h>
}

struct YUVPicture
{
	int x, y, width, height, alpha;
	AVPicture pic;
};

class InputSource
{
	int x_, y_, width_, height_, alpha_; // 画到画布的属性
	int want_x_, want_y_, want_width_, want_height_, want_alpha_;	// set_param() 设置，在 save_pic() 中修正

	ost::Mutex cs_;
	SwsContext *sws_;	// 拉伸器
	AVPicture pic_;	// 拉伸目标
	int last_width_, last_height_;	// 对应 pic_ 大小
	int last_width_valid_, last_height_valid_; // 对应最后一次 save_pic() 拉伸后的大小，一般情况下与 last_width_, last_height_ 相同
	bool idle_, valid_;  // 是否启用，是否已经 save_pic()

public:
	InputSource(void);
	~InputSource(void);

	bool idle();	// 返回是否空闲
	void employ();	// 占用，设置为非空闲
	void disemploy();	// 释放

	int save_pic(MSPicture* pic);	// 收到新的图片，拉伸，并保存
	int set_param(int x, int y, int width, int height, int alpha); // 
	YUVPicture get_pic();	// 返回最新的图片

	int x() const { return x_; }
	int y() const { return y_; }
	int width() const { return width_; }
	int height() const { return height_; }
	int alpha() const { return alpha_; }

	int want_x() const { return want_x_; }
	int want_y() const { return want_y_; }
	int want_width() const { return want_width_; }
	int want_height() const { return want_height_; }
	int want_alpha() const { return want_alpha_; }
};
