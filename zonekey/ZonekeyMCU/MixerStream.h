#pragma once

/** 比照 AudioStream, VideoStream，这个 MixerStream 是不同的。对应着视频会议中的"主席模式"
								  -----------------------
		rtp recv ---> decoder --->|						|          /--> rtp sender
		                          |						|         /
		rtp recv ---> decoder --->|  zonekey video mixer|---> tee --> rtp sender
		                          |						|        \
		rtp recv ---> decoder --->|						|         \--> rtp sender
		                          |----------------------

		更像 AudioConference 那样，支持动态增加，删除 rtp recv / rtp sender ...

		一个 VideoMixerStream 使用一个工作线程，同时支持多路 rtp recver，解码后，
		经过 zonekey.video,mixer 混合，通过 tee 输出，再经过 rtp sender 会送到 rtp recv 对应的 rtp session


		使用步骤：
			1. ZonekeyVideoMixer *mixer = new ZonekeyVideoMixer();

			2. mixer->set_encoding_setting(...); 设置编码参数

			3, VideoMixerStream *stream = mixer->addStream(...); 新增一个视频成员

			4. stream->get_canvas_size(&width, &height);	// 获取画布实际大小

			5. stream->set_channel_desc(....);			// 设置该 stream 如何显示在画布中，

			6. ..... 使用中 ....

			7. mixer->delStream(stream);	//  删除该 stream

			8. delete mixer;		//  释放所有
 */ 

#include <mediastreamer2/mediastream.h>
#include <mediastreamer2/zk.video.mixer.h>
#include "MemberStream.h"
#include <string>
#include <vector>

// 为了简单，设置最多同时 MAX_STREAMS 路，这个数值等于 tee outputs 的数目减 1，tee 剩余的一路，可以用于接受点播 :)
#define MAX_STREAMS 9

#define DEBUG_PREVIEW 1

// 对应一路流，一路流对应着一个 rtpsession + 一个 rtp recver + 一个 h264 解码器 + 一个 rtp sender
class VideoMixerStream : public MemberStream
{
	int cid_;		// 对应在 zonekey.video.mixer 中的channel id
	MSFilter *filter_decoder_;
	MSFilter *filter_mixer_;	// 用于方便直接访问 mixer 的方法

public:
	VideoMixerStream(const char *ip, int port, int cid, MSFilter *mixer_);
	~VideoMixerStream();

	// 获取画布属性
	void get_canvas_size(int *width, int *height);

	/** 设置通道属性，如画到画布的位置，透明度
	 */
	int set_channel_desc(int x, int y, int width, int height, int alpha);

	virtual MSFilter *get_recver_filter() { return filter_decoder_; }	// 输入为 h264 解码后

	/** 设置属性：必须设置有效数目的参数 :)

			name                    params						              说明
		------------     ----------------------------------     ------------------------------------------------
		 ch_desc           x, y, width, height, alpha            设置本通道属性，5个参数，所有参数都是 int 类型
		 ch_zorder		  ["up"|"down"|"top"|"bottom"|...]       设置zorder属性，一个参数，字符串类型

	 */
	virtual int set_params(const char *name, int nums, ...);
};

/** 基于 zonekey.video.mixer 的混合器
		

 */
class ZonekeyVideoMixer
{
	MSTicker *ticker_;	// 工作线程，驱动所有 graphs
	typedef std::vector<VideoMixerStream*> STREAMS;
	STREAMS streams_;
	MSFilter *filter_mixer_;	// zonekey.video.mixer
	MSFilter *filter_tee_;		// 链接 zonekey.video,mixer->outputs[1]

#if DEBUG_PREVIEW
	MSFilter *filter_sender_preview_;
	RtpSession *rtp_preview_;
#endif // debug preview

public:
	ZonekeyVideoMixer(void);
	virtual ~ZonekeyVideoMixer(void);

public:
	// 设置混合属性，主要是 h264 压缩属性
	int set_encoding_setting(int width, int height, int kbps, double fps, int gop);

	// 增加一路流，如果成功，返回 VideoStream，如果无法添加新的 stream了，返回 0
	// 注意啊：此时 stream 并没有在画布中显示，需要调用 stream->set_channel_desc() 才行的
	VideoMixerStream *addStream(const char *ip, int port);

	// 删除一路流
	void delStream(VideoMixerStream *stream);

	// 根据 cid 获取 Stream
	VideoMixerStream *getStreamFromID(int cid);

private:
	// 从 zonekey.video.mixer 申请一个新的 cid
	int allocate_channel();
	void release_channel(int cid);

	// 添加到整个 grapha 中
	int add_stream_to_graph(VideoMixerStream *stream);
	int del_stream_from_graph(VideoMixerStream *stream);
};
