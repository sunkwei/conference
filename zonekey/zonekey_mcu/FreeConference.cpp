#include "FreeConference.h"
#include <mediastreamer2/zk.publisher.h>
#include <algorithm>
#include <assert.h>

FreeConference::FreeConference(int id)
	: Conference(id)
{
}

FreeConference::~FreeConference(void)
{
	// 释放所有 sources, sinks, ...
	// 释放 Graphs
	do {
		ost::MutexLock al(cs_graphics_);
		GRAPHICS::iterator it;
		for (it = graphics_.begin(); it != graphics_.end(); ++it) {
			delete *it;
		}
	} while (0);

	do {
		ost::MutexLock al(cs_sinks_);
		SINKS::iterator it;
		for (it = sinks_.begin(); it != sinks_.end(); ++it) {
			del_sink(*it);
			delete *it;
		}
		sinks_.clear();
	} while (0);

	do {
		ost::MutexLock al(cs_sources_);
		SOURCES::iterator it;
		for (it = sources_.begin(); it != sources_.end(); ++it) {
			del_source(*it);
			delete *it;
		}
		sources_.clear();
	} while (0);
}

int FreeConference::add_source(Source *s, KVS &params)
{
	ost::MutexLock al(cs_graphics_);

	Graph *g = new Graph(s, s->source_id());
	
	graphics_.push_back(g);
	return 0;
}

int FreeConference::del_source(Source *s)
{
	ost::MutexLock al(cs_graphics_);

	// FIXME: 如果 g->sink_num() > 0 怎么办？？？

	Graph *g = 0;
	GRAPHICS::iterator it;
	for (it = graphics_.begin(); it != graphics_.end(); ++it) {
		if ((*it)->source() == s) {
			g = *it;
			graphics_.erase(it);
			break;
		}
	}

	if (g) {
		delete g;
		return 0;
	}
	else {
		return -1;	// 没有找到 s
	}
}

int FreeConference::add_sink(int sid, Sink *s, KVS &params)
{
	assert(is_source_id(sid));
	assert(find_source(sid) != 0);
	Graph *g = find_graph(sid);
	if (!g) {
		// 没有找到对应的 source
		return -1;
	}

	g->add_sink(s);
	
	return 0;
}

int FreeConference::del_sink(Sink *s)
{
	bool found = false;

	ost::MutexLock al(cs_graphics_);
	GRAPHICS::iterator it;
	for (it = graphics_.begin(); it != graphics_.end(); ++it) {
		if ((*it)->has_sink(s)) {
			(*it)->del_sink(s);
			found = true;
			break;
		}
	}

	return found ? 0 : -1;
}

FreeConference::Graph *FreeConference::find_graph(int id)
{
	ost::MutexLock al(cs_graphics_);
	GRAPHICS::iterator it;
	for (it = graphics_.begin(); it != graphics_.end(); ++it) {
		if ((*it)->id() == id)
			return *it;
	}
	return 0;
}

FreeConference::Graph::Graph(Source *s, int id)
	: id_(id)
{
	ticker_ = ms_ticker_new();
	source_ = s;

	publisher_ = ms_filter_new_from_name("ZonekeyPublisher");
	MSFilter *filter_source= s->get_output();
	
	ms_filter_link(filter_source, 0, publisher_, 0);

	ms_ticker_attach(ticker_, filter_source);
}

FreeConference::Graph::~Graph()
{
	ms_ticker_detach(ticker_, publisher_);

	ms_filter_destroy(publisher_);
	ms_ticker_destroy(ticker_);
}

int FreeConference::Graph::id() const
{
	return id_;
}

int FreeConference::Graph::sink_num()
{
	ost::MutexLock al(cs_sinks_);
	return sinks_.size();
}

Source *FreeConference::Graph::source()
{
	return source_;
}

int FreeConference::Graph::add_sink(Sink *s)
{
	assert(!has_sink(s));

	ost::MutexLock al(cs_sinks_);
	sinks_.push_back(s);

	RtpSession *rs = s->get_rtp_session();
	ms_filter_call_method(publisher_, ZONEKEY_METHOD_PUBLISHER_ADD_REMOTE, rs);

	return sinks_.size();
}

int FreeConference::Graph::del_sink(Sink *s)
{
	ost::MutexLock al(cs_sinks_);

	std::vector<Sink*>::iterator itf = std::find(sinks_.begin(), sinks_.end(), s);
	if (itf != sinks_.end()) {
		ms_filter_call_method(publisher_, ZONEKEY_METHOD_PUBLISHER_DEL_REMOTE, s->get_rtp_session());
		sinks_.erase(itf);
		return 1;
	}

	return 0;
}

bool FreeConference::Graph::has_sink(Sink *s)
{
	ost::MutexLock al(cs_sinks_);
	std::vector<Sink*>::iterator itf = std::find(sinks_.begin(), sinks_.end(), s);
	return (itf != sinks_.end());
}
