#pragma once
#include <assert.h>
#include "Conference.h"
#include <mediastreamer2/zk.video.mixer.h>
	/**       
	           ------------------------|
	           |                       |
	           V                       |
	  	stream --> tee --> mixer -----> publisher
		              \
			       \--> publisher
	 */


#define MAX_STREAMS 9

class DirectorConference : public Conference
{
public:
	DirectorConference(int id, int livingcast = 0);
	~DirectorConference();

protected:
	virtual ConferenceType type() const { return CT_DIRECTOR; }

	// 一般来说，只能添加视频源
	virtual int add_source(Source *s, KVS &params);
	virtual int del_source(Source *s);

	virtual int add_sink(int sid, Sink *s, KVS &params);
	virtual int del_sink(Sink *s);

	virtual int add_stream(Stream *s, KVS &params);
	virtual int del_stream(Stream *s);

	virtual int setParams(KVS &params, KVS &results);
	virtual int getParams(KVS &params, KVS &results);

	virtual int vs_exchange_position(int id1, int id2);

private:
	/** a graph include: stream --> tee --> publisher
                       			 |
					 ---> decoder --> mixer

	 */

	MSFilter *audio_publisher_, *video_publisher_;// publish mixer stream...
	MSFilter *audio_mixer_, *video_mixer_;	// the mixer
	MSFilter *video_tee_;	// the tee for video mixer
	MSFilter *audio_resample_, *audio_encoder_;	// 用于 audio 的 preview 输出到 audio publisher
	MSTicker *audio_ticker_, *video_ticker_;

	void pause_video();
	void resume_video();
	void pause_audio();
	void resume_audio();
	int add_audio_stream(Stream *s, KVS &param);
	int add_video_stream(Stream *s, KVS &param);
	int del_audio_stream(Stream *s);
	int del_video_stream(Stream *s);

	/// cid 为 video_mixer 返回的 channel id
	int set_video_stream_params_layout(int cid, KVS &params);

	int sp_v_desc(KVS &params, KVS &result);
	int sp_v_zorder(KVS &params, KVS &result);
	int sp_v_encoder(KVS &params, KVS &result);

	int gp_v_desc(KVS &params, KVS &result);
	int gp_v_zorder(KVS &params, KVS &result);
	int gp_v_encoder(KVS &params, KVS &result);
	int gp_v_stat(KVS &params, KVS &result);
	int gp_v_comple_state(KVS &params, KVS &result);

	int hlp_get_v_desc(Stream *vs, ZonekeyVideoMixerChannelDesc *desc);
	int hlp_set_v_desc(Stream *vs, ZonekeyVideoMixerChannelDesc *desc);

	Stream *get_v_stream_from_cid(int cid);
	
	int find_audio_free_pin();
	int find_video_free_pin();

	int audio_stream_cnt_;
	int video_stream_cnt_;

	// 对应着一路 publisher
	class Graph
	{
		int id_;
		MSTicker *ticker_;
		Source *source_;
		MSFilter *publisher_;
		std::vector<Sink*> sinks_;
		ost::Mutex cs_sinks_;

	public:
		Graph(Source *s, int id);
		~Graph();

		int add_sink(Sink *s);
		int del_sink(Sink *s);
		bool has_sink(Sink *s);
		int id() const;
		int sink_num();
		Source *source();
	};

	typedef std::vector<Graph *> GRAPHICS;
	GRAPHICS graphics_;
	ost::Mutex cs_graphics_;

	MSFilter *filter_tee_, *filter_sink_;

	// 根据 sid 找到匹配的 Graph
	Graph *find_graph(int sid);
};
