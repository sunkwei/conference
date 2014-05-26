#include <mediastreamer2/msrtp.h>
#include <assert.h>
#include "MixerStream.h"
#include "util.h"

VideoMixerStream::VideoMixerStream(const char *ip, int port, int cid, MSFilter *mixer)
	: cid_(cid)
	, filter_mixer_(mixer)
	, MemberStream(cid, SENDRECV, 100)
{

	RtpSession *rtpsess = get_rtp_session();
	rtp_session_set_remote_addr_and_port(rtpsess, ip, port, port+1);

	filter_decoder_ = ms_filter_new(MS_H264_DEC_ID);
	
	// 首先将 rtp recver 链接上 h264 decoder 
	ms_filter_link(MemberStream::get_recver_filter(), 0, filter_decoder_, 0);
}

VideoMixerStream::~VideoMixerStream()
{
	ms_filter_destroy(filter_decoder_);
}

int VideoMixerStream::set_channel_desc(int x, int y, int width, int height, int alpha)
{
	ZonekeyVideoMixerChannelDesc desc;
	desc.id = cid_;
	desc.x = x;
	desc.y = y;
	desc.width = width;
	desc.height = height;
	desc.alpha = alpha;
	return ms_filter_call_method(filter_mixer_, ZONEKEY_METHOD_VIDEO_MIXER_SET_CHANNEL_DESC, &desc);
}

void VideoMixerStream::get_canvas_size(int *width, int *height)
{
	ZonekeyVideoMixerEncoderSetting setting;
	int rc = ms_filter_call_method(filter_mixer_, ZONEKEY_METHOD_VIDEO_MIXER_GET_ENCODER_SETTING, &setting);
	if (rc == 0) {
		*width = setting.width;
		*height = setting.height;
	}
	else {
		*width = 0;
		*height = 0;
	}
}

int VideoMixerStream::set_params(const char *name, int num, ...)
{
	va_list list;

	va_start(list, num);

	if (!strcmp(name, "ch_desc")) {
		// 5个参数，都是 int 类型
		assert(num == 5);

		int x = va_arg(list, int);
		int y = va_arg(list, int);
		int width = va_arg(list, int);
		int height = va_arg(list, int);
		int alpha = va_arg(list, int);

		return set_channel_desc(x, y, width, height, alpha);
	}
	else if (!strcmp(name, "ch_zorder")) {
		// 一个字符串参数
		assert(num == 1);

		char *mode = va_arg(list, char*);
		ZonekeyVideoMixerZOrder zorder;
		zorder.id = id();

		if (!strcmp(mode, "top"))
			zorder.order_oper = ZONEKEY_VIDEO_MIXER_ZORDER_TOP;
		else if (!strcmp(mode, "bottom"))
			zorder.order_oper = ZONEKEY_VIDEO_MIXER_ZORDER_BOTTOM;
		else if (!strcmp(mode, "up"))
			zorder.order_oper = ZONEKEY_VIDEO_MIXER_ZORDER_UP;
		else if (!strcmp(mode, "down"))
			zorder.order_oper = ZONEKEY_VIDEO_MIXER_ZORDER_DOWN;
		else
			return -1;

		return ms_filter_call_method(filter_mixer_, ZONEKEY_METHOD_VIDEO_MIXER_SET_ZORDER, &zorder);
	}

	va_end(list);

	return -1;
}

ZonekeyVideoMixer::ZonekeyVideoMixer(void)
{
	ticker_ = ms_ticker_new();
	filter_tee_ = ms_filter_new(MS_TEE_ID);

	MSFilterDesc *desc = ms_filter_lookup_by_name("ZonekeyVideoMixer");
	filter_mixer_ = ms_filter_new_from_desc(desc);

	ms_filter_link(filter_mixer_, 1, filter_tee_, 0);	// 链接 mixer outputs[1] 和 tee inputs[0]

#ifdef DEBUG_PREVIEW
	// 使用 tee->outputs[9] 链接 rtp sender，发送到 127.0.0.1:50000 用于预监窗口 :)
	rtp_preview_ = rtp_session_new(RTP_SESSION_SENDONLY);
	rtp_session_set_remote_addr_and_port(rtp_preview_, "127.0.0.1", 50000, 50001);
	rtp_session_set_profile(rtp_preview_, &av_profile);
	rtp_session_set_payload_type(rtp_preview_, 100);

	// 设置 jitter buffer
	JBParameters jbp;
	jbp.adaptive = 1;
	jbp.max_packets = 500;	// 高清需要足够大
	jbp.max_size = -1;
	jbp.nom_size = jbp.min_size = 200;	// 200ms
	rtp_session_set_jitter_buffer_params(rtp_preview_, &jbp);

	filter_sender_preview_ = ms_filter_new(MS_RTP_SEND_ID);
	ms_filter_call_method(filter_sender_preview_, MS_RTP_SEND_SET_SESSION, rtp_preview_);

	ms_filter_link(filter_tee_, 9, filter_sender_preview_, 0);	// 链接到 tee 的最后一个 outputs 上
#endif// debug
}

ZonekeyVideoMixer::~ZonekeyVideoMixer(void)
{
	ms_ticker_detach(ticker_, filter_mixer_);
	ms_ticker_destroy(ticker_);
}

VideoMixerStream *ZonekeyVideoMixer::getStreamFromID(int id)
{
	for (STREAMS::iterator it = streams_.begin(); it != streams_.end(); ++it) {
		if ((*it)->id() == id)
			return *it;
	}
	return 0;
}

VideoMixerStream *ZonekeyVideoMixer::addStream(const char *ip, int port)
{
	if (streams_.size() == MAX_STREAMS)
		return 0;

	int cid = allocate_channel();
	if (cid >= 0) {
		VideoMixerStream *stream = new VideoMixerStream(ip, port, cid, filter_mixer_);
		add_stream_to_graph(stream);
		streams_.push_back(stream);
		return stream;
	}
	else 
		return 0;
}

void ZonekeyVideoMixer::delStream(VideoMixerStream *stream)
{
	bool valid = false;

	for (STREAMS::iterator it = streams_.begin(); it != streams_.end(); ++it) {
		if (*it == stream) {
			valid = true;
			streams_.erase(it);
			break;
		}
	}

	if (valid) {
		del_stream_from_graph(stream);
		delete stream;	// 不再使用！
	}
}

int ZonekeyVideoMixer::allocate_channel()
{
	int cid = -1;
	ms_filter_call_method(filter_mixer_, ZONEKEY_METHOD_VIDEO_MIXER_GET_CHANNEL, &cid);
	
	if (cid >= 0)
		return cid;
	else
		return -1;
}

void ZonekeyVideoMixer::release_channel(int cid)
{
	ms_filter_call_method(filter_mixer_, ZONEKEY_METHOD_VIDEO_MIXER_FREE_CHANNEL, &cid);
}

int ZonekeyVideoMixer::add_stream_to_graph(VideoMixerStream *s)
{
	// 需要首先中断线程
	ms_ticker_detach(ticker_, filter_mixer_);

	// 首次加入的 stream，总是设置大小为 0，这样要求 app 重新设置其在画布上的位置
	ZonekeyVideoMixerChannelDesc cd;
	cd.id = s->id();
	cd.x = 0;
	cd.y = 0;
	cd.width = 0;
	cd.height = 0;
	cd.alpha = 255;
	ms_filter_call_method(filter_mixer_, ZONEKEY_METHOD_VIDEO_MIXER_SET_CHANNEL_DESC, &cd);

	// 更新 graph
	MSFilter *decoder = s->get_recver_filter();
	ms_filter_link(decoder, 0, filter_mixer_, s->id());

	MSFilter *sender = s->get_sender_filter();
	ms_filter_link(filter_tee_, s->id(), sender, 0);

	// 恢复线程
	ms_ticker_attach(ticker_, filter_mixer_);

	return 0;
}

int ZonekeyVideoMixer::del_stream_from_graph(VideoMixerStream *s)
{
	ms_ticker_detach(ticker_, filter_mixer_);

	// 断开连接
	ms_filter_unlink(s->get_recver_filter(), 0, filter_mixer_, s->id());
	ms_filter_unlink(filter_tee_, s->id(), s->get_sender_filter(), 0);

	release_channel(s->id());

	if (!streams_.empty())
		ms_ticker_attach(ticker_, filter_mixer_); // 重新启动线程

	return 0;
}

int ZonekeyVideoMixer::set_encoding_setting(int width, int height, int kbps, double fps, int gop)
{
	ZonekeyVideoMixerEncoderSetting setting;
	setting.width = width;
	setting.height = height;
	setting.kbps = kbps;
	setting.fps = fps;
	setting.gop = gop;

	return ms_filter_call_method(filter_mixer_, ZONEKEY_METHOD_VIDEO_MIXER_SET_ENCODER_SETTING, &setting);
}
