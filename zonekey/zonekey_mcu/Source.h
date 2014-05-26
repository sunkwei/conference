/** 数据提供者：
		本质是一个 rtp recver，从网络接受数据，将数据通过 get_output() 返回的 filter 输出


 */

#pragma once

#include <mediastreamer2/mediastream.h>
#include <ortp/ortp.h>
#include "util.h"
#include "log.h"

struct SourceStat
{
	uint64_t recv;		// total bytes recved
	uint64_t hw_recv;	// payload bytes recved
	uint64_t recv_packets;	// total packets recved
	uint64_t lost_packets;	// total packets lost
};

class Source
{
public:
	Source(int id, int pt, const char *desc = 0);
	virtual ~Source(void);

	MSFilter *get_output();

	int source_id();
	const char *get_ip();
	int get_rtp_port();
	int get_rtcp_port();
	int payload_type();
	const char *desc();

	void run();	// call from Conference threading
	bool death() const; // 一般在 Conference threading 中调用，用于判断该 source 是否依然有效
	
	SourceStat stat(); // update in Conference threading

protected:
	RtpSession *rtp_sess_;	// 在构造函数中创建缺省
	std::string myip_;
	MSFilter *filter_;
	int id_;		// source id
	int pt_;
	std::string desc_;	// source 描述，创建 source 时提供
	OrtpEvQueue *evq_;	// to recv RTCP pkg.

protected:
	virtual void on_rtcp_sr(mblk_t *m);
	virtual void on_rtcp_rr(mblk_t *m);
	virtual void on_rtcp_sdes(mblk_t *m);
	virtual void on_rtcp_bye(mblk_t *m);
	virtual void on_rtcp_app(mblk_t *m);

private:
	static void sdes_item_cb(void *user_data, uint32_t csrc, rtcp_sdes_type_t t, const char *content, uint8_t content_len);
	void process_rtcp(mblk_t *m);
	SourceStat stat_;
	
	bool death_;
	double time_last_rtcp_;
};

