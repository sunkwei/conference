#pragma once

#include <cc++/thread.h>
#include <string>

/** 对应一个 usb camera + zqpkt source
	
 */
class USBCameraZqpktSource : private ost::Thread
{
	int cam_id_, tcp_port_, last_err_;
	ost::Event evt_open_, evt_close_;
	bool quit_;
	std::string url_;
	double fps_;
	int kbitrate_, width_, height_;

public:
	// 需要提供 usb camera 的编号，zqsender 的端口 ...
	USBCameraZqpktSource(int cam_id, int tcp_port);
	~USBCameraZqpktSource(void);

	// 设置编码参数，这里仅仅设置 fps, bitrate, widthxheight，其他 x264 参数，使用 x264_param_default_preset(&param, "veryfast", "zerolatency");
	void setEncoderParam(double fps, int kbitrate, int width, int height)
	{
		fps_ = fps;
		kbitrate_ = kbitrate;
		width_ = width;
		height_ = height;
	}
	
	// 获取生成的 zqpkt url
	const char *getZqpktUrl() const { return url_.c_str(); }

	// 启动采集和输出
	int Start();

	// 停止采集和输出
	void Stop();

private:
	void run();
};
