/** 一个接收者，本质上就是一个 rtp sender，将数据通过 Sink 发出

		典型的：mcu 应该总是在公网上，但一个 client 往往可能在 NAT 后面，所以是否可以采用这样的流程：

				client 申请一个点播时， mcu 返回一个发布点的 ip:port，此时，mcu 并不直接通过该节点发送，而是等待收到 client 的第一个包；
				client 收到了发布点的 ip:port，自己启动一个 rtp session，并且朝着 ip:port 发送几个包，并接收....
 */

#pragma once

#include <mediastreamer2/mediastream.h>
#include <ortp/ortp.h>
#include <string>

struct SinkStat
{
	uint64_t sent;	// total bytes sent
	uint64_t packets;	// total packets sent
	uint64_t packets_lost;	// lost packets, from RR
	uint32_t jitter;	// last jitter, from RR
};

class Sink
{
public:
	Sink(int id, int pt, const char *desc, const char *who);
	virtual ~Sink(void);

	RtpSession *get_rtp_session();
	int sink_id();
	int payload_type();
	const char *server_ip();
	int server_rtp_port();
	int server_rtcp_port();

	void process_rtcp(mblk_t *m);

	const char *desc();
	SinkStat stat();
	const char *who();	// 发送到谁，这个值在 add_sink() 时，who=xxx 指定

	void run();	// calling from Conference threading
	bool death() const; // 一般为 Conference Threading 调用，用于判断是否

protected:
	RtpSession *rtp_session_;
	int id_, pt_;
	std::string desc_, who_;
	SinkStat stat_;
	OrtpEvQueue *evq_;	// to recv RTCP pkg.
	bool death_;

	double time_last_rtcp_;	// 最后收到的 rtcp 包，用于检测是否超时
};
