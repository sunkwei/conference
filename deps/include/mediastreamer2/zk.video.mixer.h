/** zonekey video mixer filter，本 filter 目的为了方便实现视频会议中的“主席模式”

		主席模式：每路 rtp sender 都是相同的压缩结果；
		rtp recv --> h264 decoder  \                        
		rtp recv --> h264 decoder  --> zonekey video mixer ---> YUV sink (outputs[0] 输出 yuv 预览)
		rtp recv --> h264 decoder  /         ^            \
		...           ...                    |             \--> tee (outputs[1] 输出h264码流，一般接上 tee filter，分发到每个接收者，或者发布到 fms 上供点播)
											 |		  
										method call	  
							     （修改 x264 压缩参数，调整参与者画面之类的.... ）			              


 */

#ifndef _zonekey_video_mixer__hh
#define _zonekey_video_mixer__hh

#ifdef __cplusplus
extern "C" {
#endif // c++

#include "allfilters.h"
#include "mediastream.h"

// 用于 ms_filter_lookup_by_name()
#define ZONEKEY_VIDEO_MIXER_NAME		"ZonekeyVideoMixer"

// 同时支持的最大混合
#define ZONEKEY_VIDEO_MIXER_MAX_CHANNELS 20

// 输出
#define ZONEKEY_VIDEO_MIXER_OUTPUT_PREVIEW 0
#define ZONEKEY_VIDEO_MIXER_OUTPUT_H264 1

// 一个 channel 的属性，通过 ZONEKEY_METHOD_VIDEO_MIXER_SET_CHANNEL_DESC 设置
typedef struct ZonekeyVideoMixerChannelDesc
{
	int id;		// 出参，用于指定对应的编号 [0..ZONEKEY_VIDEO_MIXER_MAX_CHANNELS)
	int x, y;	// 目标，画到画布中的左上角像素
	int width, height;	// 目标大小
	int alpha;	// 透明度，[0..255]
} ZonekeyVideoMixerChannelDesc;

enum ZonekeyVideoMixerZOrderOper
{
	ZONEKEY_VIDEO_MIXER_ZORDER_UP,
	ZONEKEY_VIDEO_MIXER_ZORDER_DOWN,
	ZONEKEY_VIDEO_MIXER_ZORDER_TOP,
	ZONEKEY_VIDEO_MIXER_ZORDER_BOTTOM,
};

// 指定 id 的通道的 z-order
typedef struct ZonekeyVideoMixerZOrder
{
	int id;	// channel 编号
	ZonekeyVideoMixerZOrderOper order_oper;	// 执行的 order 操作
} ZonekeyVideoMixerZOrder;

// 所有 channel 的 zorder 顺序
typedef struct ZonekeyVideoMixerZorderArray
{
	int orders[ZONEKEY_VIDEO_MIXER_MAX_CHANNELS+1];	// 每个为 chid，如果 -1 说明后面没有了，总是后面的覆盖前面的 :)
} ZonekeyVideoMixerZorderArray;

// 画布/压缩属性
typedef struct ZonekeyVideoMixerEncoderSetting
{
	int width, height;		// 画布大小，直接对应着压缩输出的 h264 的大小
	double fps;				// 帧率
	int kbps;				// 码率
	int gop;				// 相当于关键帧间隔，如果 fps = 25, gop = 50，则每隔两秒出一个关键帧
	// .... 其他 x264 的参数都可以在这里设置
} ZonekeyVideoMixerEncoderSetting;

// 运行状态信息
typedef struct ZonekeyVideoMixerStats
{
	int64_t sent_bytes;		// 发送的字节数
	int64_t sent_frames;	// 发送的帧数

	double avg_fps;		// 平均帧率
	double avg_kbps;	// 平均码率

	double last_fps;	// 最新 5秒平均帧率
	double last_kbps;	// 最新 5秒钟平均码率
	double last_qp;		// 最新 5秒 x264 压缩平均 qp，一般来说，这个值维持在 25 以内，图像效果都是能够接受的，如果超过 30，说明码率设置太低了！

	int last_delta;		// 最新的 delta 值，绝对值越小越好 :)

	int active_input;	// 活跃的输入者数目

	double time;	// 启动到此的时间，秒数
	
} ZonekeyVideoMixerStats;

#define ZONEKEY_ID_VIDEO_MIXER (MSFilterInterfaceBegin+12)

// method id
// 申请并且占用一个空闲通道，返回通道编号，对应 id
#define ZONEKEY_METHOD_VIDEO_MIXER_GET_CHANNEL				MS_FILTER_METHOD(ZONEKEY_ID_VIDEO_MIXER, 0, int)

// 释放占用的通道
#define ZONEKEY_METHOD_VIDEO_MIXER_FREE_CHANNEL				MS_FILTER_METHOD(ZONEKEY_ID_VIDEO_MIXER, 1, int)

// 设置通道属性
#define ZONEKEY_METHOD_VIDEO_MIXER_SET_CHANNEL_DESC			MS_FILTER_METHOD(ZONEKEY_ID_VIDEO_MIXER, 2, ZonekeyVideoMixerChannelDesc)
#define ZONEKEY_METHOD_VIDEO_MIXER_GET_CHANNEL_DESC			MS_FILTER_METHOD(ZONEKEY_ID_VIDEO_MIXER, 3, ZonekeyVideoMixerChannelDesc)

// 设置混合模式，窗口 zorder
#define ZONEKEY_METHOD_VIDEO_MIXER_SET_ZORDER				MS_FILTER_METHOD(ZONEKEY_ID_VIDEO_MIXER, 4, ZonekeyVideoMixerZOrder)

// 设置/获取混合模式，编码输出设置
#define ZONEKEY_METHOD_VIDEO_MIXER_SET_ENCODER_SETTING		MS_FILTER_METHOD(ZONEKEY_ID_VIDEO_MIXER, 5, ZonekeyVideoMixerEncoderSetting)
#define ZONEKEY_METHOD_VIDEO_MIXER_GET_ENCODER_SETTING		MS_FILTER_METHOD(ZONEKEY_ID_VIDEO_MIXER, 6, ZonekeyVideoMixerEncoderSetting)

// 获取运行状态信息
#define ZONEKEY_METHOD_VIDEO_MIXER_GET_STATS				MS_FILTER_METHOD(ZONEKEY_ID_VIDEO_MIXER, 7, ZonekeyVideoMixerStats)

// 返回当前 zorder 顺序
#define ZONEKEY_METHOD_VIDEO_MIXER_GET_ZOEDER				MS_FILTER_METHOD(ZONEKEY_ID_VIDEO_MIXER, 8, ZonekeyVideoMixerZorderArray)

// 是否启用，如果 0，则不进行任何压缩，直接将数据抛弃
#define ZONEKEY_METHOD_VIDEO_MIXER_ENABLE					MS_FILTER_METHOD(ZONEKEY_ID_VIDEO_MIXER, 9, int)

MS2_PUBLIC void zonekey_video_mixer_register();
MS2_PUBLIC void zonekey_video_mixer_set_log_handler(void (*func)(const char *, ...));

#ifdef __cplusplus
}
#endif // c++

#endif /* zk.video.mixer.h */
