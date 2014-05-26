#include "InputSource.h"
#include <assert.h>

InputSource::InputSource(void)
{
	x_ = y_ = width_ = height_ = 16;
	alpha_ = 255;	// 缺省不透明
	sws_ = 0;
	avpicture_alloc(&pic_, PIX_FMT_YUV420P, 160, 160);

	want_width_ = want_height_ = 160;
	last_width_ = last_height_ = 160;
	x_ = want_x_ = 0, y_ = want_y_ = 0;

	idle_ = true;
	valid_ = false;
}

InputSource::~InputSource(void)
{
	if (sws_) {
		sws_freeContext(sws_);
		sws_ = 0;
	}

	avpicture_free(&pic_);
}

int InputSource::save_pic(MSPicture* pic)
{
	ost::MutexLock al(cs_);

	if (idle_)
		return 0;

	if (want_width_ == 0 || want_height_ == 0 || pic->w == 0 || pic->h == 0)
		return 0;

	valid_ = true;

	if (want_width_ != width_ || want_height_ != height_ || pic->w != last_width_ || pic->h != last_height_) {
		// 需要重置 sws
		if (sws_) sws_freeContext(sws_);

		avpicture_free(&pic_);
		avpicture_alloc(&pic_, PIX_FMT_YUV420P, want_width_, want_height_);

		sws_ = sws_getContext(pic->w, pic->h, PIX_FMT_YUV420P, want_width_, want_height_, PIX_FMT_YUV420P, SWS_FAST_BILINEAR, 0, 0, 0);
		
		last_width_ = pic->w;
		last_height_ = pic->h;
		width_ = want_width_;
		height_ = want_height_;
	}

	x_ = want_x_, y_ = want_y_;
	alpha_ = want_alpha_;

	sws_scale(sws_, pic->planes, pic->strides, 0, pic->h, pic_.data, pic_.linesize);

	return 0;
}

bool InputSource::idle()
{
	ost::MutexLock al(cs_);
	return idle_;
}

void InputSource::employ()
{
	ost::MutexLock al(cs_);
	idle_ = false;
	valid_ = false;
}

void InputSource::disemploy()
{
	ost::MutexLock al(cs_);
	idle_ = true;
}

int InputSource::set_param(int x, int y, int width, int height, int alpha)
{
	ost::MutexLock al(cs_);
	
	assert(!idle());

	want_x_ = x, want_y_ = y, want_width_ = width, want_height_ = height, want_alpha_ = alpha;

	return 0;
}

YUVPicture InputSource::get_pic()
{
	ost::MutexLock al(cs_);

	YUVPicture pic;
	pic.x = x_, pic.y = y_;
	pic.width = width_;
	pic.height = height_;
	pic.alpha = alpha_;

	if (idle_ || !valid_) {
		pic.width = pic.height = 0;
		return pic;
	}

	avpicture_alloc(&pic.pic, PIX_FMT_YUV420P, pic.width, pic.height);

	// 复制 pic_ 到 pic.pic
	unsigned char *p = pic_.data[0];
	unsigned char *q = pic.pic.data[0];
	for (int i = 0; i < pic.height; i++) {
		memcpy(q, p, pic.width);
		q += pic.pic.linesize[0];
		p += pic_.linesize[0];
	}

	p = pic_.data[1];
	q = pic.pic.data[1];
	for (int i = 0; i < pic.height/2; i++) {
		memcpy(q, p, pic.width/2);
		q += pic.pic.linesize[1];
		p += pic_.linesize[1];
	}

	p = pic_.data[2];
	q = pic.pic.data[2];
	for (int i = 0; i < pic.height/2; i++) {
		memcpy(q, p, pic.width/2);
		q += pic.pic.linesize[2];
		p += pic_.linesize[2];
	}

	return pic;
}
