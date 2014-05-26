#pragma once

#include <mediastreamer2/mediastream.h>
#include <mediastreamer2/msconference.h>
#include <mediastreamer2/zk.video.mixer.h>
#include <mediastreamer2/zk.publisher.h>
#include "zonekeyconference.h"
#include "ZonekeyStreamVideoMixer.h"

/** 最多混合通道数

		这里利用了 Tee filter，tee filter 提供 10 个 outputs，对应的，0--8 用于 stream 的回传，9 用于链接 publisher，对应无数的 sinks
 */
#define MAX_MIXER_CHANNELS 9

/** 主席模式：使用音频混音 + 视频混合
		
			最多支持 9 路视频混合，9 路音频混合
 */
class ZonekeyConferenceDirector : public ZonekeyConference
{
public:
	ZonekeyConferenceDirector(void);
	~ZonekeyConferenceDirector(void);

protected:
	virtual ZonekeyStream *createStream(RtpSession *rs, KVS &params, int id);
	virtual void freeStream(ZonekeyStream *s);

	virtual ZonekeySink *createSink(RtpSession *rs, KVS &params, int id);
	virtual void freeSink(ZonekeySink *s);

	// 需要实现不少命令 ...
	virtual int set_params(const char *prop, KVS &params);
	virtual int get_params(const char *prop, KVS &results);

private:
	int insert_video_stream(ZonekeyStreamVideoMixer *s);
	int remove_video_stream(ZonekeyStreamVideoMixer *s);
	int add_video_sink_to_publisher(ZonekeySink *s);
	int del_video_sink_from_publisher(ZonekeySink *s);
	int setting_vencoder(int width, int height, int kbps, double fps, int gop);
	int setting_vstream_desc(int id, int x, int y, int width, int height, int alpha);
	int setting_vstream_zorder(int id, const char *mode);

private:
	MSAudioConference *audio_;		// 音频混合

	// mixer 输出到 tee，将 tee 链接到 
	MSFilter *video_mixer_filter_, *video_tee_filter_;
	MSTicker *ticker_;		// 用于驱动整个视频 graph
	MSFilter *video_publisher_filter_, *audio_publisher_filter_;
};
