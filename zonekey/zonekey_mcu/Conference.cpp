#include "Conference.h"
#include "log.h"
#include "H264Source.h"
#include "SpeexSource.h"
#include "H264Sink.h"
#include "SpeexSink.h"
#include <algorithm>
#include <assert.h>
#include "H264Stream.h"
#include "SpeexStream.h"
#include "PCMuStream.h"
#include "iLBC_Stream.h"
#include "ilbcSink.h"
#include <zonekey/zkrobot_mse_ex.h>

Conference::Conference(int id)
	: cid_(id)
{
	sid_ = 1;			// source id 总是奇数
	sink_id_ = 0;		
	stream_id_ = 2;		// stream id 总是偶数

	desc_ = "";
	idle_ = false;
	time_begin_ = util_time_now();
	stamp_idle_ = util_time_now();
}

Conference::~Conference(void)
{
	// 等待的通知必须都主动发出，否则，zkrobot 库会内存泄露
	ost::MutexLock al(cs_reg_notify_tokens_);
	REG_NOTIFY_TOKENS::iterator it;
	for (it = reg_notify_tokens_.begin(); it != reg_notify_tokens_.end(); ++it) {
		std::deque<void*>::iterator it2;
		for (it2 = it->second.begin(); it2 != it->second.end(); ++it2) {
			zkrbt_mse_respond(*it2, "err", 0);
		}
	}
}

int Conference::addSource(KVS &params, KVS &results)
{
	ost::MutexLock al(cs_sources_);

	char info[1024];

	if (!chk_params(params, info, "payload", 0)) {
		log("[Conference] %s: err, '%s'\n", __FUNCTION__, info);
		return -1;
	}

	int payload = atoi(params["payload"].c_str());
	std::string desc = "";

	if (chk_params(params, info, "desc", 0))
		desc = params["desc"];

	if (payload != 100 && payload != 110) {
		log("Conference] %s: err, ONLY support payload=100: h264 and 110 speex wb!\n", __FUNCTION__);
		return -1;
	}

	Source *source = 0;
	if (payload == 100) {
		// 创建 h264 rtp recver
		H264Source *s = new H264Source(sid_, desc.c_str());
		source = s;
	}
	else {
		// 创建 speex wb rtp recver
		SpeexSource *s = new SpeexSource(sid_, desc.c_str());
		source = s;
	}

	int rc = add_source(source, params);
	if (rc < 0) {
		// 失败
		delete source;
		return rc;
	}

	// ok，保存 sid, server_ip, server_rtp_port, server_rtcp_port
	char buf[128];
	snprintf(buf, sizeof(buf), "%d", source->source_id());
	results["sid"] = buf;
	snprintf(buf, sizeof(buf), "%d", source->get_rtp_port());
	results["server_rtp_port"] = buf;
	snprintf(buf, sizeof(buf), "%d", source->get_rtcp_port());
	results["server_rtcp_port"] = buf;
	results["server_ip"] = source->get_ip();

	sources_.push_back(source);

	sid_ += 2;
	if (sid_ >= 0x70000000) sid_ = 1;	// 防止负数 :)，一般不太可能这么大的

	return 0;
}

Source *Conference::find_source(int sid)
{
	ost::MutexLock al(cs_sources_);

	SOURCES::iterator it;
	for (it = sources_.begin(); it != sources_.end(); ++it) {
		if ((*it)->source_id() == sid) {
			return *it;
		}
	}
	return 0;
}

Stream *Conference::find_stream(int sid)
{
	ost::MutexLock al(cs_streams_);

	STREAMS::iterator it;
	for (it = streams_.begin(); it != streams_.end(); ++it) {
		if ((*it)->stream_id() == sid) {
			return *it;
		}
	}
	return 0;
}

int Conference::delSource(KVS &params, KVS &results)
{
	ost::MutexLock al(cs_sources_);

	char info[1024];
	if (!chk_params(params, info, "sid", 0)) {
		log("[Conference] %s: 'sid' need\n", __FUNCTION__);
		return -1;
	}

	int id = atoi(params["sid"].c_str());
	Source *s = 0;

	SOURCES::iterator it;
	for (it = sources_.begin(); it != sources_.end(); ++it) {
		if ((*it)->source_id() == id) {
			s = *it;
			sources_.erase(it);
			break;
		}
	}

	if (s) {
		del_source(s);
		delete s;
		return 0;
	}
	else
		return -1;
}

int Conference::addSink(KVS &params, KVS &results)
{
	ost::MutexLock al(cs_sinks_);

	char info[1024];
	if (!chk_params(params, info, "sid", 0)) {
		log("[Conference] %s: 'sid' need\n", __FUNCTION__);
		return -1;
	}

	int sid = atoi(params["sid"].c_str());
	int payload = -1;

	if (chk_params(params, info, "payload", 0))
		payload = atoi(params["payload"].c_str());

	if (type() == CT_FREE) {
		Source *source = find_source(sid);
		if (!source) {
			log("[Conference] %s: not found sid=%d\n", 
					__FUNCTION__, sid);
			return -1;
		}
		payload = source->payload_type();
	}
	else if (type() == CT_DIRECTOR) {
		if (payload == -1) {
			if (is_source_id(sid)) {
				Source *source = find_source(sid);
				if (!source) {
					log("[Conference] %s: not found sid=%d\n", 
							__FUNCTION__, sid);
					return -1;
				}
				payload = source->payload_type();
			}
			else {
				Stream *stream = find_stream(sid);
				if (!stream) {
					log("[Conference %s: not found sid=%d\n",
							__FUNCTION__, sid);
					return -1;
				}
				payload = stream->payload_type();
			}
		}
	}

	std::string desc = "";
	if (chk_params(params, info, "desc", 0))
		desc = params["desc"];

	std::string who = "";
	if (chk_params(params, info, "who", 0))
		who = params["who"];

	sink_id_++;
	if (sink_id_ > 0x70000000) sink_id_ = 0;

	Sink *sink = 0;
	if (payload == 100) {
		H264Sink *s = new H264Sink(sink_id_, desc.c_str(), who.c_str());
		sink = s;
	}
	else if (payload == 110) {
		SpeexSink *s = new SpeexSink(sink_id_, desc.c_str(), who.c_str());
		sink = s;
	}
	else if (payload == 102) {
		iLBCSink *s = new iLBCSink(sink_id_, desc.c_str(), who.c_str());
		sink = s;
	}

	if (sink == 0) {
		results["info"] = "NOT support payload type";
		return -1;
	}

	int rc = add_sink(sid, sink, params);
	if (rc < 0) {
		// 失败
		delete sink;
		return -1;
	}

	// ok，保存 sinkid, server_ip, server_rtp_port, server_rtcp_port
	char buf[128];
	snprintf(buf, sizeof(buf), "%d", sink->sink_id());
	results["sinkid"] = buf;
	snprintf(buf, sizeof(buf), "%d", sink->server_rtp_port());
	results["server_rtp_port"] = buf;
	snprintf(buf, sizeof(buf), "%d", sink->server_rtcp_port());
	results["server_rtcp_port"] = buf;
	results["server_ip"] = sink->server_ip();

	sinks_.push_back(sink);

	return 0;
}

int Conference::delSink(KVS &params, KVS &results)
{
	ost::MutexLock al(cs_sinks_);

	char info[1024];
	if (!chk_params(params, info, "sinkid", 0)) {
		log("[Conference] %s: 'sinkid' need\n", __FUNCTION__);
		return -1;
	}

	int id = atoi(params["sinkid"].c_str());

	Sink *sink = 0;
	SINKS::iterator it;
	for (it = sinks_.begin(); it != sinks_.end(); ++it) {
		if ((*it)->sink_id() == id) {
			sink = *it;
			sinks_.erase(it);
			break;
		}
	}

	if (sink) {
		del_sink(sink);
		delete sink;
		return 0;
	}
	else
		return -1;
}

static Stream *makeStream(int payload, int stream_id, const char *desc, bool sink, bool publisher)
{
	if (payload == 100) {
		return new H264Stream(stream_id, desc, sink, publisher);
	}
	else if (payload == 110) {
		return new SpeexStream(stream_id, desc, sink, publisher);
	}
	else if (payload == 0) {
		return new PCMuStream(stream_id, desc, sink, publisher);
	}
	else if (payload == 102) {
		return new iLBC_Stream(stream_id, desc, sink, publisher);
	}

	return 0;
}

int Conference::addStream(int payload, const char *desc, bool sink, bool publisher, KVS &params, KVS &results)
{
	Stream *stream = makeStream(payload, stream_id_, desc, sink, publisher);

	int rc = add_stream(stream, params);
	if (rc < 0) {
		delete stream;
		return -1;
	}

	char buf[128];
	snprintf(buf, sizeof(buf), "%d", stream->stream_id());
	results["streamid"] = buf;
	snprintf(buf, sizeof(buf), "%d", stream->server_rtp_port());
	results["server_rtp_port"] = buf;
	snprintf(buf, sizeof(buf), "%d", stream->server_rtcp_port());
	results["server_rtcp_port"] = buf;
	results["server_ip"] = stream->server_ip();

	streams_.push_back(stream);

	stream_id_ += 2;
	if (stream_id_ >= 0x7f000000)
		stream_id_ = 0;

	return 0;
}

int Conference::addStream(KVS &params, KVS &results)
{
	ost::MutexLock al(cs_streams_);
	assert(type() == CT_DIRECTOR);

	char info[1024];
	if (!chk_params(params, info, "payload", 0)) {
		log("[Conference] %s: 'payload' need!\n", __FUNCTION__);
		return -1;
	}

	int payload = atoi(params["payload"].c_str());

	std::string desc = "";
	if (chk_params(params, info, "desc", 0))
		desc = params["desc"];

	if (payload != 100 && payload != 110 && payload != 0 && payload != 102) {
		log("Conference] %s: err, ONLY support payload=100(h264), 110(speex wb), 102(iLBC), and 0(PCMu)!\n", __FUNCTION__);
		return -1;
	}

	return addStream(payload, desc.c_str(), true, false, params, results);
}

int Conference::addStreamWithPublisher(KVS &params, KVS &results)
{
	ost::MutexLock al(cs_streams_);
	assert(type() == CT_DIRECTOR);

	char info[1024];
	if (!chk_params(params, info, "payload", 0)) {
		log("[Conference] %s: 'payload' need!\n", __FUNCTION__);
		return -1;
	}

	int payload = atoi(params["payload"].c_str());

	std::string desc = "";
	if (chk_params(params, info, "desc", 0))
		desc = params["desc"];

	if (payload != 100 && payload != 110 && payload != 0 && payload != 102) {
		log("Conference] %s: err, ONLY support payload=100(h264), 110(speex wb), 102(iLBC), and 0(PCMu)!\n", __FUNCTION__);
		return -1;
	}

	/// FIXME: 此时简化了，不支持 sink
	return addStream(payload, desc.c_str(), false, true, params, results);
}

int Conference::delStream(KVS &params, KVS &results)
{
	ost::MutexLock al(cs_streams_);
	char info[1024];
	if (!chk_params(params, info, "streamid", 0)) {
		log("[Conference] %s: 'streamid' need!\n", __FUNCTION__);
		return -1;
	}

	int id = atoi(params["streamid"].c_str());

	Stream *s = 0;
	STREAMS::iterator it;
	for (it = streams_.begin(); it != streams_.end(); ++it) {
		if ((*it)->stream_id() == id) {
			s = *it;
			streams_.erase(it);
			break;
		}
	}

	if (s) {
		del_stream(s);
		delete s;
		return 0;
	}
	else
		return -1;
}

int Conference::waitEvent(const char *from, const char *cmd, const char *options, KVS &params, KVS &results, void *uac_token)
{
	/** 当增加/删除 sources/streams 时，触发通知
	 */
	// 注册一个通知
	XmppResponse res;
	res.token = uac_token;
	res.from = from;
	res.cmd = cmd;
	if (options) res.cmd_options = options;
	res.result = "ok";	// FIXME: 

	XMPP_NOTIFY_DATAS::iterator itf = xmpp_notify_datas_.find(from);
	if (itf != xmpp_notify_datas_.end()) {
		itf->second.tokens.push_back(res);
	}
	else {
		XmppNotifyData data;
		data.tokens.push_back(res);
		xmpp_notify_datas_[from] = data;
	}

	return 0;
}

int Conference::reg_notify(const char *from_jid, void *token)
{
	ost::MutexLock al(cs_reg_notify_tokens_);
	REG_NOTIFY_TOKENS::iterator itf = reg_notify_tokens_.find(from_jid);
	if (itf == reg_notify_tokens_.end()) {
		std::deque<void*> list;
		list.push_back(token);
		reg_notify_tokens_[from_jid] = list;
	}
	else {
		itf->second.push_back(token);
	}
	return 0;
}

int Conference::chkCastNotify(const char *who, const char *cmd, const char *options, const char *result, const char *result_options)
{
	/** 在 xmpp callback 中调用
	 */
	KVS opts;
	opts["who"] = who;
	opts["cmd"] = cmd;
	opts["cmd_options"] = options ? options : "<null>";
	opts["result"] = result;
	opts["result_options"] = result_options ? result_options : "<null>";

	XMPP_NOTIFY_DATAS::iterator it;
	for (it = xmpp_notify_datas_.begin(); it != xmpp_notify_datas_.end(); ) {
		if (!it->second.tokens.empty()) {
			XmppResponse res = it->second.tokens.front();
			it->second.tokens.pop_front();
			res.result_options = util_encode_options(opts);
			if (res.token) zkrbt_mse_respond(res.token, res.result.c_str(), res.result_options.c_str());
		}

		if (!it->second.tokens.empty())
			xmpp_notify_datas_.erase(it++);
		else
			++it;
	}

	return 0;
}

std::vector<SourceDescription> Conference::get_source_descriptions(int payload)
{
	std::vector<SourceDescription> descs;
	ost::MutexLock al(cs_sources_);
	SOURCES::iterator it;
	for (it = sources_.begin(); it != sources_.end(); ++it) {
		if (payload == -1 || payload == (*it)->payload_type()) {
			SourceDescription desc;
			desc.sid = (*it)->source_id();
			desc.desc = (*it)->desc();
			desc.stats = (*it)->stat();
			desc.payload = (*it)->payload_type();

			descs.push_back(desc);
		}
	}
	return descs;
}

std::vector<SinkDescription> Conference::get_sink_descriptions(int payload)
{
	std::vector<SinkDescription> descs;
	ost::MutexLock al(cs_sinks_);
	SINKS::iterator it;
	for (it = sinks_.begin(); it != sinks_.end(); ++it) {
		if (payload == -1 || payload == (*it)->payload_type()) {
			SinkDescription desc;
			desc.sinkid = (*it)->sink_id();
			desc.desc = (*it)->desc();
			desc.who = (*it)->who();
			desc.stats = (*it)->stat();
			desc.payload = (*it)->payload_type();

			descs.push_back(desc);
		}
	}
	return descs;
}

std::vector<StreamDescription> Conference::get_stream_descriptions(int payload)
{
	std::vector<StreamDescription> descs;
	ost::MutexLock al(cs_streams_);
	STREAMS::iterator it;
	for (it = streams_.begin(); it != streams_.end(); ++it) {
		if (payload == -1 || payload == (*it)->payload_type()) {
			StreamDescription desc;
			desc.streamid = (*it)->stream_id();
			desc.desc = (*it)->desc();
			desc.stats = (*it)->stat();
			desc.payload = (*it)->payload_type();

			descs.push_back(desc);
		}
	}
	return descs;
}

template<class T>
class pred_zero
{
public:
	bool operator() (const T &x) const
	{
		return x == 0;
	}
};

void Conference::run_once()
{
	size_t n_streams, n_sources, n_sinks;

	do {
		ost::MutexLock al(cs_sources_);
		n_sources = sources_.size();
		SOURCES::iterator it;
		for (it = sources_.begin(); it != sources_.end(); ++it) {
			if ((*it)->death()) {
				log("[Conference] %s: SOURCE DEATH: sid=%d, desc=%s\n", __FUNCTION__, (*it)->source_id(), (*it)->desc());
				del_source(*it);
				delete *it;
				*it = 0;
			}
			else
				(*it)->run();
		}
		pred_zero<Source*> pred;
		SOURCES::iterator it_end = std::remove_if(sources_.begin(), sources_.end(), pred);
		sources_.erase(it_end, sources_.end());
	} while (0);

	do {
		ost::MutexLock al(cs_sinks_);
		n_sinks = sinks_.size();
		SINKS::iterator it;
		for (it = sinks_.begin(); it != sinks_.end(); ++it) {
			if ((*it)->death()) {
				log("[Conference] %s: SINK DEATH: sid=%d, desc=%s\n", __FUNCTION__, (*it)->sink_id(), (*it)->desc());
				del_sink(*it);
				delete *it;
				*it = 0;
			}
			else
				(*it)->run();
		}
		pred_zero<Sink*> pred;
		sinks_.erase(std::remove_if(sinks_.begin(), sinks_.end(), pred), sinks_.end());
	} while (0);

	do {
		ost::MutexLock al(cs_streams_);
		n_streams = streams_.size();
		STREAMS::iterator it;
		for (it = streams_.begin(); it != streams_.end(); ++it) {
			if ((*it)->death()) {
				log("[Conference] %s: STREAM DEATH: sid=%d, desc=%s\n", __FUNCTION__, (*it)->stream_id(), (*it)->desc());
				del_stream(*it);
				delete *it;
				*it = 0;
			}
			else
				(*it)->run();
		}
		pred_zero<Stream*> pred;
		STREAMS::iterator it_end = std::remove_if(streams_.begin(), streams_.end(), pred);
		streams_.erase(it_end, streams_.end());
	} while (0);

	// 此时判断是否空闲
	// FIXME：这里是否需要关心 sinks ??
	bool idle = (n_sources == 0) && (n_streams == 0);

	double now = util_time_now();
	if (idle && now - stamp_idle_ > 120.0) {

		log("[Conference] cid=%d, IDLE, should be destroied!!!!\n", cid());

		idle_ = true;	// 真的空闲了 :)
	}

	if (!idle)
		stamp_idle_ = now;
}

bool Conference::idle()
{
	return idle_;
}
