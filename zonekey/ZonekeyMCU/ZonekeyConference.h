
#pragma once

#include <mediastreamer2/mediastream.h>
#include <ortp/ortp.h>
#include <map>
#include "ZonekeySink.h"
#include "ZonekeyStream.h"
#include "util.h"

// 会议模式，主席，自由...
enum ZonekeyConferenceMode
{
	CM_DIRCETOR,	// 导播模式：导播模式中，所有成员的视频由导播决定混合，压缩，发给每个成员
	CM_FREE,		// 自由模式：每个成员自行选择希望看的内容
};

/** 会议的基本接口：

	分成两类成员：
		一类是数据提供者（参与者），使用 add_stream()/del_stream() 接口，
		一类仅仅是数据使用着（观众），使用 add_sink()/del_sink() 接口

 */
class ZonekeyConference
{
public:
	ZonekeyConference(ZonekeyConferenceMode mode);
	virtual ~ZonekeyConference(void);

	/** 添加双向通讯实例，params 一般为解析 sdp 后得到的key,value

			目前要求：
			    client_ip: 
				client_port:
				payload: xxx

			TODO:	也可能包含候选者，需要协商
	 */
	ZonekeyStream *add_stream(KVS &params);
	void del_stream(ZonekeyStream *stream);
	ZonekeyStream *get_stream(int id);

	// 添加删除接收者，这些只是普通观众
	ZonekeySink *add_sink(KVS &params);
	void del_sink(ZonekeySink *sink);
	ZonekeySink *get_sink(int id);
	
	// 设置属性
	virtual int set_params(const char *prop, KVS &params)
	{
		return -1;
	}

	// 获取属性
	virtual int get_params(const char *prop, KVS &results)
	{
		return -1;
	}

	// 返回会议模式
	ZonekeyConferenceMode mode() const { return mode_; }

protected:
	// 派生类必须实现，在 add_stream() 中调用
	virtual ZonekeyStream *createStream(RtpSession *sess, KVS &params, int id) = 0;
	virtual void freeStream(ZonekeyStream *stream) = 0;

	// 必须在派生类中实现，在 add_sink()中调用
	virtual ZonekeySink *createSink(RtpSession *rtp_session, KVS &params, int id) = 0;
	virtual void freeSink(ZonekeySink *sink) = 0;

protected:
	ZonekeyConferenceMode mode_;

private:
	typedef std::map<int, ZonekeyStream*> STREAMS;
	STREAMS map_streams_;

	typedef std::map<int, ZonekeySink*> SINKS;
	SINKS map_sinks_;

	int stream_id_, sink_id_;		// 
};
