/** 描述一个成员的一路流，如一路音频流，或者一路视频流
	本质上，一路流包含：
			rtp session: 用于收发 rtp 数据，根据类型，可能 SENDONLY, RECVONLY, SENDRECV 三种
			rtp recver filter: 用于接收来自client的数据
			rtp sender filter: 用于发送到 client 的数据
 */

#pragma once

#include <mediastreamer2/mediastream.h>
#include "util.h"

enum MemberStreamType
{
	SENDONLY,
	RECVONLY,
	SENDRECV,
};

class MemberStream
{
public:
	MemberStream(int id, MemberStreamType type, int payload);
	virtual ~MemberStream(void);

	// 不同类型的 MemberStream，可能输出不同的 sender 或者 recver
	virtual MSFilter *get_sender_filter() { return filter_sender_; }
	virtual MSFilter *get_recver_filter() { return filter_recver_; }
	virtual int id() const { return id_; }

	// 设置属性，派生类自己实现支持吧
	virtual int set_params(const char *name, int num, ...) 
	{
		return 0;
	}

	RtpSession *get_rtp_session() { return rtp_session_; }
	const char *get_server_ip() { return util_get_myip(); }
	int get_server_port() 
	{ 
		if (type_ == RECVONLY || type_ == SENDRECV)
			return rtp_session_get_local_port(rtp_session_); 
		else
			return 0;
	}

private:
	MSFilter *filter_sender_, *filter_recver_;
	RtpSession *rtp_session_;
	MemberStreamType type_;
	int id_;
};
