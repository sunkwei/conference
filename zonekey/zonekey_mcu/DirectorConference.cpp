#include "DirectorConference.h"
#include <assert.h>
#include <algorithm>
#include "util.h"
#include <mediastreamer2/mediastream.h>
#include <mediastreamer2/zk.video.mixer.h>
#include <mediastreamer2/zk.audio.mixer.h>
#include <mediastreamer2/zk.publisher.h>
#include "log.h"
#include <mediastreamer2/msaudiomixer.h>

DirectorConference::DirectorConference(int id, int livingcast)
	: Conference(id)
{
	filter_tee_ = ms_filter_new(MS_TEE_ID);
	filter_sink_ = ms_filter_new_from_name("ZonekeyVoidSink");

	log("%s: ... id=%d \n", __FUNCTION__, id);

#if 1
	audio_mixer_ = ms_filter_new_from_name("ZonekeyAudioMixer");
	int rate = 16000;
	ms_filter_call_method(audio_mixer_, MS_FILTER_SET_SAMPLE_RATE, &rate);
#else
	audio_mixer_ = ms_filter_new(MS_AUDIO_MIXER_ID);
	int tmp = 1, rate = 16000;
	ms_filter_call_method(audio_mixer_, MS_AUDIO_MIXER_ENABLE_CONFERENCE_MODE, &tmp);
	ms_filter_call_method(audio_mixer_, MS_FILTER_SET_SAMPLE_RATE, &rate);
#endif
	video_mixer_ = ms_filter_new_from_name("ZonekeyVideoMixer");
	audio_publisher_ = ms_filter_new_from_name("ZonekeyPublisher");
	video_publisher_ = ms_filter_new_from_name("ZonekeyPublisher");
	audio_resample_ = ms_filter_new(MS_RESAMPLE_ID);
	audio_encoder_ = ms_filter_new_from_name("MSIlbcEnc");
	video_tee_ = ms_filter_new(MS_TEE_ID);

	// 是否启用 video mixer
	ms_filter_call_method(video_mixer_, ZONEKEY_METHOD_VIDEO_MIXER_ENABLE, (void*)livingcast);

	audio_ticker_ = ms_ticker_new();
	video_ticker_ = ms_ticker_new();

	/// 总是将 audio mixer 的 preview 使用 iLBC 输出
	int in_sample = 16000, in_ch = 1;
	int out_sample = 8000, out_ch = 1;
	ms_filter_call_method(audio_resample_, MS_FILTER_SET_SAMPLE_RATE, &in_sample);
	ms_filter_call_method(audio_resample_, MS_FILTER_SET_NCHANNELS, &in_ch);
	ms_filter_call_method(audio_resample_, MS_FILTER_SET_OUTPUT_SAMPLE_RATE, &out_sample);
	ms_filter_call_method(audio_resample_, MS_FILTER_SET_OUTPUT_NCHANNELS, &out_ch);

	// simple link
	ms_filter_link(audio_mixer_, ZONEKEY_AUDIO_MIXER_PREVIEW_PIN, audio_resample_, 0);
	ms_filter_link(audio_resample_, 0, audio_encoder_, 0);
	ms_filter_link(audio_encoder_, 0, audio_publisher_, 0);

	ms_filter_link(video_mixer_, 1, video_tee_, 0);
	ms_filter_link(video_tee_, MAX_STREAMS, video_publisher_, 0);

	audio_stream_cnt_ = 0;
	video_stream_cnt_ = 0;

	resume_audio();
	resume_video();

	log("ok\n");
}

DirectorConference::~DirectorConference()
{
	// 释放所有 sources, sinks, ...
	do {
		ost::MutexLock al(cs_graphics_);
		GRAPHICS::iterator it;
		for (it = graphics_.begin(); it != graphics_.end(); ++it) {
			delete *it;
		}
		graphics_.clear();
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
		ost::MutexLock al(cs_streams_);
		STREAMS::iterator it;
		for (it = streams_.begin(); it != streams_.end(); ++it) {
			del_stream(*it);
			delete *it;
		}
		streams_.clear();
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

	pause_audio();
	pause_video();

	ms_ticker_destroy(audio_ticker_);
	ms_ticker_destroy(video_ticker_);

	ms_filter_destroy(audio_resample_);
	ms_filter_destroy(audio_encoder_);
	ms_filter_destroy(audio_mixer_);
	ms_filter_destroy(video_mixer_);
	ms_filter_destroy(audio_publisher_);
	ms_filter_destroy(video_publisher_);
	ms_filter_destroy(video_tee_);

	if (filter_tee_) ms_filter_destroy(filter_tee_);
	if (filter_sink_) ms_filter_destroy(filter_sink_);
}

void DirectorConference::pause_audio()
{
	ms_ticker_detach(audio_ticker_, audio_mixer_);
}

void DirectorConference::resume_audio()
{
	if (audio_stream_cnt_ > 0)
		ms_ticker_attach(audio_ticker_, audio_mixer_);
}

void DirectorConference::pause_video()
{
	ms_ticker_detach(video_ticker_, video_mixer_);
}

void DirectorConference::resume_video()
{
	if (video_stream_cnt_ > 0)
		ms_ticker_attach(video_ticker_, video_mixer_);
}

int DirectorConference::add_stream(Stream *s, KVS &params)
{
	// FIXME: now, just pin mixer ...
	if (s->payload_type() == 100)
		return add_video_stream(s, params);
	else if (s->payload_type() == 110 || s->payload_type() == 0 || s->payload_type() == 102)
		return add_audio_stream(s, params);
	else
		return -1;
}

int DirectorConference::del_stream(Stream *s)
{
	// FIXME: now, just unpin mixer ...
	if (s->payload_type() == 100)
		return del_video_stream(s);
	else if (s->payload_type() == 110 || s->payload_type() == 0 || s->payload_type() == 102)
		return del_audio_stream(s);
	else
		return -1;
}

int DirectorConference::add_sink(int sid, Sink *s, KVS &params)
{
	// 
	if (sid == -1) {
		if (s->payload_type() == 100) {
			ms_filter_call_method(video_publisher_, ZONEKEY_METHOD_PUBLISHER_ADD_REMOTE, s->get_rtp_session());
		}
		else if (s->payload_type() == 102) {
			ms_filter_call_method(audio_publisher_, ZONEKEY_METHOD_PUBLISHER_ADD_REMOTE, s->get_rtp_session());
		}
		else if (s->payload_type() == 110) {
			ms_filter_call_method(audio_publisher_, ZONEKEY_METHOD_PUBLISHER_ADD_REMOTE, s->get_rtp_session());
		}
		return 0;
	}
	else if (is_source_id(sid)) {
		assert(find_source(sid) != 0);
		Graph *g = find_graph(sid);
		if (!g) {
			// 没有找到对应的 source
			return -1;
		}

		g->add_sink(s);
	
		return 0;
	}
	else {
		// 说明对 Stream 的点播
		Stream *stream = find_stream(sid);
		if (!stream || !stream->support_publisher()) {
			// 没有找到或者不支持 publisher
			return -1;
		}
		else {
			stream->add_sink(s);
			return 0;
		}
	}
}

int DirectorConference::del_sink(Sink *s)
{
	// FIXME: 似乎应该提供更好的方式，及早识别 s 的属性
	bool found = false;

	// Source 的点播
	do {
		ost::MutexLock al(cs_graphics_);
		GRAPHICS::iterator it;
		for (it = graphics_.begin(); it != graphics_.end(); ++it) {
			if ((*it)->has_sink(s)) {
				(*it)->del_sink(s);
				found = true;
				break;
			}
		}
	} while (0);

	// Stream 的点播
	do {
		ost::MutexLock al(cs_streams_);
		for (STREAMS::iterator it = streams_.begin(); it != streams_.end(); ++it) {
			(*it)->del_sink(s);
		}
	} while (0);
	
	// 全局的点播
	do {
		if (s->payload_type() == 100) {
			ms_filter_call_method(video_publisher_, ZONEKEY_METHOD_PUBLISHER_DEL_REMOTE, s->get_rtp_session());
		}
		else {
			ms_filter_call_method(audio_publisher_, ZONEKEY_METHOD_PUBLISHER_DEL_REMOTE, s->get_rtp_session());
		}
	} while (0);

	return 0;
}

int DirectorConference::find_audio_free_pin()
{
	// now, the ticker detached !
	// just return idle outputs
	for (int i = 0; i < MAX_STREAMS; i++) {
		if (audio_mixer_->outputs[i] == 0)
			return i;
	}

	assert(0);
	return -1;
}

int DirectorConference::find_video_free_pin()
{
	// now, the ticker detached!
	int cid = -1;
	ms_filter_call_method(video_mixer_, ZONEKEY_METHOD_VIDEO_MIXER_GET_CHANNEL, &cid);
	assert(cid >= 0);
	return cid;
}

int DirectorConference::add_audio_stream(Stream *s, KVS &params)
{
	if (audio_stream_cnt_ >= MAX_STREAMS) {
		log("[Conference] %s: Oh, toooo many audio streams!\n",
				__FUNCTION__);
		return -2;
	}

	pause_audio();
	int cid = find_audio_free_pin();
	s->channel_id(cid);	// save for removing

	// connect audio decoder to mixer
	ms_filter_link(s->get_source_filter(), s->get_source_pin(), audio_mixer_, cid);

	if (s->payload_type() == -100) {
		fprintf(stderr, "ERR: ??? stream payload = -100 ????\n");
		__asm int 3;
		ms_filter_link(audio_mixer_, cid, filter_tee_, 0);
		if (s->support_sink())
			ms_filter_link(filter_tee_, 0, s->get_sink_filter(), 0);
		ms_filter_link(filter_tee_, 1, filter_sink_, 0);
	}
	else {
		// connect mixer to stream sender
		if (s->support_sink())
			ms_filter_link(audio_mixer_, cid, s->get_sink_filter(), 0);
	}
	audio_stream_cnt_++;
	resume_audio();

	return 0;
}

int DirectorConference::del_audio_stream(Stream *s)
{
	pause_audio();
	int cid = s->channel_id();

	ms_filter_unlink(s->get_source_filter(), 0, audio_mixer_, cid);
	if (s->support_sink())
		ms_filter_unlink(audio_mixer_, cid, s->get_sink_filter(), 0);

	audio_stream_cnt_--;
	resume_audio();

	return 0;
}

/// 如果 params 中有 dx,dy,dwidth,dheight，则使用这些参数进行布局(注意，为 百分比！！！），
/// 如果无，则使用缺省 9 分屏布局
int DirectorConference::set_video_stream_params_layout(int cid, KVS &params)
{
	ZonekeyVideoMixerEncoderSetting curr_setting;
	ms_filter_call_method(video_mixer_, ZONEKEY_METHOD_VIDEO_MIXER_GET_ENCODER_SETTING, &curr_setting);
	int canvas_width = curr_setting.width, canvas_height = curr_setting.height;
	
	char info[1024];
	if (chk_params(params, info, "dx", "dy", "dwidth", "dheight", 0)) {
		int x = atoi(params["dx"].c_str());
		int y = atoi(params["dy"].c_str());
		int width = atoi(params["dwidth"].c_str());
		int height = atoi(params["dheight"].c_str());
		int alpha = 255;
		if (chk_params(params, info, "alpha", 0))
			alpha = atoi(params["alpha"].c_str());

		ZonekeyVideoMixerChannelDesc cd;
		cd.id = cid;
		cd.width = width * canvas_width / 100;
		cd.height = height * canvas_height / 100;
		cd.x = x * canvas_width / 100;
		cd.y = y * canvas_height / 100;
		cd.alpha = alpha;
		ms_filter_call_method(video_mixer_, ZONEKEY_METHOD_VIDEO_MIXER_SET_CHANNEL_DESC, &cd);
		return 1;
	}
	else {
		// 设置缺省位置和属性，最多支持 9 路视频，则，缺省按照 9 共格模式显示, 
		//          0  1  2
		//          3  4  5
		//          6  7  8

		ZonekeyVideoMixerChannelDesc cd;
		cd.id = cid;
		cd.width = canvas_width / 3;
		cd.height = canvas_height /3 ;
		cd.x = (cid % 3) * cd.width;
		cd.y = (cid / 3) * cd.height;
		cd.alpha = 255;
		ms_filter_call_method(video_mixer_, ZONEKEY_METHOD_VIDEO_MIXER_SET_CHANNEL_DESC, &cd);
		return 0;
	}
}

int DirectorConference::add_video_stream(Stream *s, KVS &params)
{
	if (video_stream_cnt_ >= MAX_STREAMS) {
		log("[Conference] %s: Oh, tooooo many video streams!\n",
				__FUNCTION__);
		return -2;
	}
	pause_video();
	int cid = find_video_free_pin();
	s->channel_id(cid);	// save for removing

	log("[Conference] %s: using mixer ch id %d\n", __FUNCTION__, cid);

	// connect video decoder to mixer
	ms_filter_link(s->get_source_filter(), 0, video_mixer_, cid);
	// connect the mixer output to stream sender
	if (s->support_sink())
		ms_filter_link(video_tee_, cid, s->get_sink_filter(), 0);

	// 检查 params 中是否有位置信息，如果有，则设置布局
	set_video_stream_params_layout(cid, params);

	video_stream_cnt_++;
	resume_video();
	return 0;
}

int DirectorConference::del_video_stream(Stream *s)
{
	pause_video();
	int cid = s->channel_id();

	ms_filter_call_method(video_mixer_, ZONEKEY_METHOD_VIDEO_MIXER_FREE_CHANNEL, &cid);

	ms_filter_unlink(s->get_source_filter(), 0, video_mixer_, cid);
	if (s->support_sink())
		ms_filter_unlink(video_tee_, cid, s->get_sink_filter(), 0);

	video_stream_cnt_--;
	resume_video();

	return 0;
}

int DirectorConference::setParams(KVS &params, KVS &results)
{
	char info[128];

	if (!chk_params(params, info, "name", 0)) {
		log("[Conference] %s: the key 'name' NEED!\n", __FUNCTION__);
		results["reason"] = info;
		return -1;
	}

	const char *name = params["name"].c_str();
	if (!strcmp(name, "v.desc")) {
		// to set video source's x, y, width, height
		return sp_v_desc(params, results);
	}
	else if (!strcmp(name, "v.zorder")) {
		// to set video zorder, oper
		return sp_v_zorder(params, results);
	}
	else if (!strcmp(name, "v.encoder")) {
		return sp_v_encoder(params, results);
	}

	std::stringstream ss;
	ss << "unsupported param: name='" << name << "'";
	results["reason"] = ss.str();

	return -1;
}

int DirectorConference::getParams(KVS &params, KVS &results)
{
	char info[128];

	if (!chk_params(params, info, "name", 0)) {
		log("[Conference] %s: the key 'name' NEED!\n", __FUNCTION__);
		results["reason"] = info;
		return -1;
	}

	const char *name = params["name"].c_str();
	if (!strcmp(name, "v.desc")) {
		return gp_v_desc(params, results);
	}
	else if (!strcmp(name, "v.zorder")) {
		return gp_v_zorder(params, results);
	}
	else if (!strcmp(name, "v.encoder")) {
		return gp_v_encoder(params, results);
	}
	else if (!strcmp(name, "v.stat")) {
		return gp_v_stat(params, results);
	}
	else if (!strcmp(name, "v.complete_state")) {
		return gp_v_comple_state(params, results);
	}

	return -1;
}

#define PARAM_I(p,k) atoi(p[k].c_str())
#define PARAM_S(p,k) (p[k].c_str())

int DirectorConference::sp_v_desc(KVS &params, KVS &results)
{
	char info[128];
	if (chk_params(params, info, "streamid", "x", "y", "width", "height", "alpha", 0)) {
		int sid = PARAM_I(params, "streamid");
		int x = PARAM_I(params, "x");
		int y = PARAM_I(params, "y");
		int width = PARAM_I(params, "width");
		int height = PARAM_I(params, "height");
		int alpha = PARAM_I(params, "alpha");

		Stream *s = find_stream(sid);
		if (!s) {
			results["reason"] = "can't find the streamid";
			return -1;
		}
		else {
			ZonekeyVideoMixerChannelDesc desc;
			desc.id = s->channel_id();
			desc.x = x, desc.y = y;
			desc.width = width, desc.height = height;
			desc.alpha = alpha;
		
			ms_filter_call_method(video_mixer_, ZONEKEY_METHOD_VIDEO_MIXER_SET_CHANNEL_DESC, &desc);

			return 0;
		}
	}
	else {
		std::string ss;
		ss = "for 'v.desc' the 'streamid', 'x', 'y', 'width', 'height' and 'alpha' MUST supply";
		results["reason"] = ss;
		return -1;
	}
}

int DirectorConference::sp_v_encoder(KVS &params, KVS &results)
{
	char info[128];
	if (!chk_params(params, info, "width", "height", "fps", "kbps", "gop", 0)) {
		results["reason"] = info;
		return -1;
	}
	else {
		ZonekeyVideoMixerEncoderSetting setting;
		setting.width = atoi(params["width"].c_str());
		setting.height = atoi(params["height"].c_str());
		setting.fps = atof(params["fps"].c_str());
		setting.gop = atoi(params["gop"].c_str());
		setting.kbps = atoi(params["kbps"].c_str());

		ms_filter_call_method(video_mixer_, ZONEKEY_METHOD_VIDEO_MIXER_SET_ENCODER_SETTING, &setting);
		return 0;
	}
}

int DirectorConference::sp_v_zorder(KVS &params, KVS &results)
{
	char info[128];
	if (chk_params(params, info, "streamid", "oper", 0)) {
		int id = PARAM_I(params, "streamid");
		const char *soper = PARAM_S(params, "oper");
		Stream *s = find_stream(id);
		if (!s) {
			results["reason"] = "can't find the streamid";
			return -1;
		}
		else {
			ZonekeyVideoMixerZOrderOper oper = ZONEKEY_VIDEO_MIXER_ZORDER_TOP;
			if (!strcmp("top", soper))
				oper = ZONEKEY_VIDEO_MIXER_ZORDER_TOP;
			else if (!strcmp("up", soper))
				oper = ZONEKEY_VIDEO_MIXER_ZORDER_UP;
			else if (!strcmp("down", soper))
				oper = ZONEKEY_VIDEO_MIXER_ZORDER_DOWN;
			else if (!strcmp("bottom", soper))
				oper = ZONEKEY_VIDEO_MIXER_ZORDER_BOTTOM;
			else {
				results["reason"] = "for 'v.zorder', only support 'up'/'down'/'top'/'bottom'!";
				return -1;
			}

			ZonekeyVideoMixerZOrder o;
			o.id = s->channel_id();
			o.order_oper = oper;

			ms_filter_call_method(video_mixer_, ZONEKEY_METHOD_VIDEO_MIXER_SET_ZORDER, &o);

			return 0;
		}
	}
	else {
		results["reason"] = "for 'v.zorder' the 'streamid' and 'oper' MUST supply!";
		return -1;
	}
}

int DirectorConference::gp_v_desc(KVS &param, KVS &result)
{
	char info[128];
	if (!chk_params(param, info, "streamid", 0)) {
		result["reason"] = "for 'v.desc', the streamid MUST supply!";
		return -1;
	}
	else {
		int id = PARAM_I(param, "streamid");
		Stream *s = find_stream(id);
		if (!s) {
			result["reason"] = "can't find the streamid";
			return -1;
		}
		else {
			ZonekeyVideoMixerChannelDesc desc;
			desc.id = s->channel_id();

			if (ms_filter_call_method(video_mixer_, ZONEKEY_METHOD_VIDEO_MIXER_GET_CHANNEL_DESC, &desc) < 0) {
				result["reason"] = "zonekey video mixer err :(";
				return -1;
			}

			char sv[16];
			snprintf(sv, sizeof(sv), "%d", desc.x);
			result["x"] = sv;

			snprintf(sv, sizeof(sv), "%d", desc.y);
			result["y"] = sv;

			snprintf(sv, sizeof(sv), "%d", desc.width);
			result["width"] = sv;

			snprintf(sv, sizeof(sv), "%d", desc.height);
			result["height"] = sv;

			snprintf(sv, sizeof(sv), "%d", desc.alpha);
			result["alpha"] = sv;

			return 0;
		}
	}
}

int DirectorConference::hlp_get_v_desc(Stream *vs, ZonekeyVideoMixerChannelDesc *desc)
{
	desc->id = vs->channel_id();
	if (ms_filter_call_method(video_mixer_, ZONEKEY_METHOD_VIDEO_MIXER_GET_CHANNEL_DESC, desc) < 0) {
		return -1;
	}

	return 0;
}

int DirectorConference::hlp_set_v_desc(Stream *vs, ZonekeyVideoMixerChannelDesc *desc)
{
	desc->id = vs->channel_id();
	if (ms_filter_call_method(video_mixer_, ZONEKEY_METHOD_VIDEO_MIXER_SET_CHANNEL_DESC, desc) < 0) {
		return -1;
	}

	return 0;
}

int DirectorConference::gp_v_encoder(KVS &param, KVS &result)
{
	ZonekeyVideoMixerEncoderSetting setting;
	if (ms_filter_call_method(video_mixer_, ZONEKEY_METHOD_VIDEO_MIXER_GET_ENCODER_SETTING, &setting) < 0) {
		result["reason"] = "zonekey video mixer err :(";
		return -1;
	}
	else {
		char sv[16];
		snprintf(sv, sizeof(sv), "%d", setting.width);
		result["width"] = sv;

		snprintf(sv, sizeof(sv), "%d", setting.height);
		result["height"] = sv;

		snprintf(sv, sizeof(sv), "%f", setting.fps);
		result["fps"] = sv;

		snprintf(sv, sizeof(sv), "%d", setting.kbps);
		result["kbps"] = sv;

		snprintf(sv, sizeof(sv), "%d", setting.gop);
		result["gop"] = sv;

		return 0;
	}
}

int DirectorConference::gp_v_stat(KVS &param, KVS &result)
{
	ZonekeyVideoMixerStats stat;
	if (ms_filter_call_method(video_mixer_, ZONEKEY_METHOD_VIDEO_MIXER_GET_STATS, &stat) < 0) {
		result["reason"] = "zonekey video mixer err :(";
		return -1;
	}
	else {
		char sv[32];
#ifdef WIN32
#	define I64FMT "%I64d"
#else
#	define I64FMT "%lld"
#endif
		snprintf(sv, sizeof(sv), I64FMT, stat.sent_bytes);
		result["sent_bytes"] = sv;

		snprintf(sv, sizeof(sv), I64FMT, stat.sent_frames);
		result["sent_frames"] = sv;

		snprintf(sv, sizeof(sv), "%f", stat.avg_fps);
		result["avg_fps"] = sv;

		snprintf(sv, sizeof(sv), "%f", stat.avg_kbps);
		result["avg_kbps"] = sv;

		snprintf(sv, sizeof(sv), "%f", stat.last_fps);
		result["last_fps"] = sv;

		snprintf(sv, sizeof(sv), "%f", stat.last_kbps);
		result["last_kbps"] = sv;

		snprintf(sv, sizeof(sv), "%f", stat.last_qp);
		result["last_qp"] = sv;

		snprintf(sv, sizeof(sv), "%d", stat.last_delta);
		result["last_delta"] = sv;

		snprintf(sv, sizeof(sv), "%d", stat.active_input);
		result["active_input"] = sv;

		snprintf(sv, sizeof(sv), "%f", stat.time);
		result["up_time"] = sv;

		return 0;
	}
}

Stream *DirectorConference::get_v_stream_from_cid(int cid)
{
	ost::MutexLock al(cs_streams_);
	for (STREAMS::iterator it = streams_.begin(); it != streams_.end(); ++it) {
		if ((*it)->payload_type() == 100 && (*it)->channel_id() == cid)
			return *it;
	}
	return 0;
}

int DirectorConference::gp_v_zorder(KVS &params, KVS &result)
{
	ZonekeyVideoMixerZorderArray arr;
	if (ms_filter_call_method(video_mixer_, ZONEKEY_METHOD_VIDEO_MIXER_GET_ZOEDER, &arr) < 0) {
		result["reason"] = "zonekey video mixer err :(";
		return -1;
	}
	else {
		/** FIXME: 这里直接需要直接访问基类 :(
		 */
		std::stringstream ss;
		for (int i = 0; i < ZONEKEY_VIDEO_MIXER_MAX_CHANNELS; i++) {
			Stream *s = get_v_stream_from_cid(arr.orders[i]);
			if (s) {
				ss << s->stream_id() << ",";
			}
		}
		ss << "-1";

		result["zorder_array"] = ss.str();

		return 0;
	}
}

int DirectorConference::gp_v_comple_state(KVS &params, KVS &results)
{
	/** 获取完整的 video streams 信息
	     每行一个 video stream，使用 \n 分割
		 后面行的 zorder 在前面
		 每行的格式为：

				streamid channelid x y width height alpha \n
	 */
	std::stringstream ss;

	ZonekeyVideoMixerZorderArray arr;
	ms_filter_call_method(video_mixer_, ZONEKEY_METHOD_VIDEO_MIXER_GET_ZOEDER, &arr);
	for (int i = 0; i < ZONEKEY_VIDEO_MIXER_MAX_CHANNELS && arr.orders[i] != -1; i++) {
		int cid = arr.orders[i];
		Stream *s = get_v_stream_from_cid(cid);
		if (s) {
			ZonekeyVideoMixerChannelDesc ch_desc;
			ch_desc.id = cid;
			ms_filter_call_method(video_mixer_, ZONEKEY_METHOD_VIDEO_MIXER_GET_CHANNEL_DESC, &ch_desc);
			ss << s->stream_id() << " " << cid << " " << ch_desc.x << " " << ch_desc.y << " "
				<< ch_desc.width << " " << ch_desc.height << " " << ch_desc.alpha << "\n";
		}
	}

	results["state"] = ss.str();

	return 0;
}

int DirectorConference::add_source(Source *s, KVS &params)
{
	ost::MutexLock al(cs_graphics_);

	Graph *g = new Graph(s, s->source_id());
	
	graphics_.push_back(g);
	return 0;
}

int DirectorConference::del_source(Source *s)
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

DirectorConference::Graph *DirectorConference::find_graph(int id)
{
	ost::MutexLock al(cs_graphics_);
	GRAPHICS::iterator it;
	for (it = graphics_.begin(); it != graphics_.end(); ++it) {
		if ((*it)->id() == id)
			return *it;
	}
	return 0;
}

DirectorConference::Graph::Graph(Source *s, int id)
	: id_(id)
{
	ticker_ = ms_ticker_new();
	source_ = s;

	publisher_ = ms_filter_new_from_name("ZonekeyPublisher");
	MSFilter *filter_source= s->get_output();
	
	ms_filter_link(filter_source, 0, publisher_, 0);

	ms_ticker_attach(ticker_, filter_source);
}

DirectorConference::Graph::~Graph()
{
	ms_ticker_detach(ticker_, publisher_);

	ms_filter_destroy(publisher_);
	ms_ticker_destroy(ticker_);
}

int DirectorConference::Graph::id() const
{
	return id_;
}

int DirectorConference::Graph::sink_num()
{
	ost::MutexLock al(cs_sinks_);
	return sinks_.size();
}

Source *DirectorConference::Graph::source()
{
	return source_;
}

int DirectorConference::Graph::add_sink(Sink *s)
{
	assert(!has_sink(s));

	ost::MutexLock al(cs_sinks_);
	sinks_.push_back(s);

	RtpSession *rs = s->get_rtp_session();
	ms_filter_call_method(publisher_, ZONEKEY_METHOD_PUBLISHER_ADD_REMOTE, rs);

	return sinks_.size();
}

int DirectorConference::Graph::del_sink(Sink *s)
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

bool DirectorConference::Graph::has_sink(Sink *s)
{
	ost::MutexLock al(cs_sinks_);
	std::vector<Sink*>::iterator itf = std::find(sinks_.begin(), sinks_.end(), s);
	return (itf != sinks_.end());
}

int DirectorConference::vs_exchange_position(int id1, int id2)
{
	Stream *s1 = find_stream(id1), *s2 = find_stream(id2);
	if (s1 && s2 && s1->payload_type() == 100 && s2->payload_type() == 100) {
		ZonekeyVideoMixerChannelDesc desc1, desc2;
		if (hlp_get_v_desc(s1, &desc1) == 0 && hlp_get_v_desc(s2, &desc2) == 0) {
			hlp_set_v_desc(s1, &desc2);
			hlp_set_v_desc(s2, &desc1);
		}

		return 0;
	}
	else 
		return -1;
}
