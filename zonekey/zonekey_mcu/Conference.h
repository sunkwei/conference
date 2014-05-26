/** 对于一个会议来说，分为两种情况：
        主席模式：此时大家看到的是经过导播控制的输出；
		自由模式：相当于点播

	对于参与者分为：
		贡献者：提供数据  ----> Source
		观众：使用数据，贡献者未必是观众   ----> Sink
 */

#pragma once

#include "Source.h"
#include "Sink.h"
#include "util.h"
#include "Stream.h"
#include <cc++/thread.h>
#include <deque>
#include <map>
#include <string>

enum ConferenceType
{
	CT_FREE,
	CT_DIRECTOR,
};

struct SourceDescription
{
	int sid;
	std::string desc;
	SourceStat stats;
	int payload;
};

struct SinkDescription
{
	int sinkid;
	const char *desc;
	const char *who;
	SinkStat stats;
	int payload;
};

struct StreamDescription
{
	int streamid;
	std::string desc;
	StreamStat stats;
	int payload;
};

struct XmppResponse
{
	void *token;
	std::string cmd, cmd_options;
	std::string result, result_options;
	std::string from;
};

/** 实现XmppNotify，当需要通知时，广播到所有接收者
 */
struct XmppNotifyData
{
	std::deque<XmppResponse> tokens;
};
typedef std::map<std::string, XmppNotifyData> XMPP_NOTIFY_DATAS;

class Server;

/** addSource/delSource: 添加删除数据贡献者
	addSink/delSink: 添加删除观众

	参数和执行结果，通过 KVS 传递.
 */
class Conference
{
	friend class Server;

public:
	Conference(int id);
	virtual ~Conference(void);

	/** 设置会议描述信息
	 */
	void set_desc(const char *desc)
	{
		if (desc) desc_ = desc;
	}

	// 由 Server 调用，不能阻塞！！！！
	// 这只是提供一个机会 :)
	void run_once();

	// 返回该 Conference 是否已经空闲？就是说，没有 sources, sinks(?), streams
	bool idle();

	const char *desc() const { return desc_.c_str(); }

	/** 在 xmpp 工作线程中调用
		会议中新增一个数据源。
		params 中包含：
			payload: 必须：100 或者 110，100 表示h264，110 表示 speex wb；
			desc: 可选：源描述
			
		如果成功，results 中包含：
			sid: source id，用于唯一表示该路 source
			server_ip: server ip
			server_rtp_port：server rtp 接收端口
			server_rtcp_port: server rtcp 接收端口

	 */
	int addSource(KVS &params, KVS &results);

	/** 在 xmpp 工作线程中调用
		删除会议中的数据源
		params 中包含:
			sid: source id
			
	 */
	int delSource(KVS &params, KVS &results);

	/** 在 xmpp 工作线程中调用
		新增一个会议观众
		params 中包含：
			sid: source id，希望观看那一路source，如果主席模式，该值无效？

		如果成功，results 包含:
			sinkid: sink id
			server_ip: server ip
			server_rtp_port：server rtp 接收端口
			server_rtcp_port: server rtcp 接收端口
	 */
	int addSink(KVS &params, KVS &results);
	int delSink(KVS &params, KVS &results);

	// 在 xmpp 工作线程中调用
	int addStream(KVS &params, KVS &results);
	int addStreamWithPublisher(KVS &params, KVS &results);
	int delStream(KVS &params, KVS &results);

	/** 在 xmpp 工作线程中调用，当处理完了 addSink/addStream/addSource/delSink/delStream/delSource/setParams 后调用
	 */
	int chkCastNotify(const char *who, const char *cmd, const char *options, const char *result, const char *result_options);

	// 在 xmpp 线程中调用，意思是，client 发出一个异步请求，当 mcu 返回这个请求时（通过 zkrbt_mse_respond()），client将收到 response
	int waitEvent(const char *from, const char *cmd, const char *options, KVS &params, KVS &results, void *uac_token);
	int reg_notify(const char *from_jid, void *uac_token);

	virtual int setParams(KVS &params, KVS &results)
	{
		return 0;
	}
	virtual int getParams(KVS &params, KVS &results)
	{
		return 0;
	}

	int cid() const
	{
		return cid_;
	}

	/// 返回互动启动的时间，秒
	double uptime() const
	{
		return util_time_now() - time_begin_;
	}

	virtual ConferenceType type() const = 0;

	// to get all source's description
	std::vector<SourceDescription> get_source_descriptions(int payload = -1);
	std::vector<SinkDescription> get_sink_descriptions(int payload = -1);
	std::vector<StreamDescription> get_stream_descriptions(int payload = -1);

protected:

	/** 新增一个数据源，将数据源插入 graph 中
		返回 < 0 失败
	 */
	virtual int add_source(Source *s, KVS &params) = 0;

	/** 删除一个 source
	 */
	virtual int del_source(Source *s) = 0;

	/** 添加一个接受者，

	  	sid: the Source id or Stream id
	 */
	virtual int add_sink(int sid, Sink *sink, KVS &params) = 0;

	/** 删除一个接收者
	 */
	virtual int del_sink(Sink *s) = 0;

	virtual int add_stream(Stream *s, KVS &params) = 0;
	virtual int del_stream(Stream *s) = 0;

	/** 交 id1, id2 的视频位置，仅仅导播模式实现，缺省认为成功
	 */
	virtual int vs_exchange_position(int id1, int id2) { return 0; }

	// 用于 add_sink 时判断 sid 是 Source 还是 Stream
	bool is_source_id(int sid)
	{
		return (sid % 2) == 1;
	}

protected:
	typedef std::vector<Source*> SOURCES;
	SOURCES sources_;
	ost::Mutex cs_sources_;
	
	typedef std::vector<Sink*> SINKS;
	SINKS sinks_;
	ost::Mutex cs_sinks_;

	typedef std::vector<Stream*> STREAMS;
	STREAMS streams_;
	ost::Mutex cs_streams_;
	std::string desc_;

private:
	double time_begin_;	// 互动启动时间
	int sid_, sink_id_, stream_id_;		// 用于生成唯一的 id，sid_ 总是奇数， stream_id_ 总是偶数
	int cid_;	// 会议 id

	XMPP_NOTIFY_DATAS xmpp_notify_datas_;
	ost::Mutex cs_xmpp_notify_data_;

	typedef std::map<std::string, std::deque<void*> > REG_NOTIFY_TOKENS;
	REG_NOTIFY_TOKENS reg_notify_tokens_;
	ost::Mutex cs_reg_notify_tokens_;

	int addStream(int payload, const char *desc, bool sink, bool publisher, KVS &params, KVS &result);

	bool idle_;	// 是否空闲
	double stamp_idle_;	// 最后 idle 的时间，当联系 idle 超过 2 分钟，则认为此 conference 无人了

protected:
	Source *find_source(int sid);
	Stream *find_stream(int sid);
};
