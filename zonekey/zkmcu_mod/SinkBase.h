#pragma once

#include <mediastreamer2/mediastream.h>
#include <ortp/ortp.h>
#include <mediastreamer2/msrtp.h>

/// 从 mcu 的 sink 接收媒体数据
class SinkBase
{
	void (*cb_data_)(void *opaque, double stamp, void *data, int len, bool key);
	void *opaque_;
	std::string mcu_ip_;
	int mcu_rtp_port_, mcu_rtcp_port_, payload_;
	MSTicker *ticker_;
	MSFilter *f_recv_, *f_void_;	// rtp recv filter 和 zonekey void filter
	RtpSession *rtp_;
	bool first_frame_;
	uint32_t last_stamp_;	// 用于处理时间戳回绕问题
	double next_stamp_;	// 使用秒的当前时间戳，
	ost::Mutex cs_;

public:
	/// data 为回调函数，当收到数据（完整帧）后，将被调用，注意，为内部线程 ...
	SinkBase(int payload, const char *mcu_ip, int mcu_rtp_port, int mcu_rtcp_port, void (*data)(void *opaque, double stamp, void *data, int len, bool key), void *opaque);
	virtual ~SinkBase(void);

	// 启动 ...
	int Start();
	int Stop();

	// 外部驱动，用于完成周期执行的动作，如处理 rtcp 等
	void RunOnce();

protected:
	virtual double payload_freq() const = 0;	// 返回对应类型的时间戳频率，一般来说， h264 使用 90000.0，而 ilbc 使用 8000.0
	virtual MSQueue *post_handle(mblk_t *im) = 0;	// 当收到数据后的处理

	virtual MSFilter *trans_in() { return 0; }
	virtual MSFilter *trans_out() { return 0; }

	virtual int size_for(int index, mblk_t *im) { return im->b_wptr - im->b_rptr; }
	virtual void save_for(int index, mblk_t *im, unsigned char *buf) { memcpy(buf, im->b_rptr, im->b_wptr - im->b_rptr); }
	virtual bool is_key(int index, mblk_t *im) { return true; }	// 音频都是 key

	MSQueue queue_;

private:
	static void cb_data(void *opaque, mblk_t *data)
	{
		((SinkBase*)opaque)->cb_data(data);
	}

	void cb_data(mblk_t *data);
};
