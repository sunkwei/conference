#ifndef _zkrtsp_client__hh
#define _zkrtsp_client__hh

/** 基于 live555，提供一个 rtsp client 接口，方便通过 rtsp 获取媒体流，对于我们来说
	实现 video: h264, audio: aac+speex 即可

	基本使用流程：
		zkrtspclient_t *c = zkrtspc_open(url);	// url: rtsp:// ....
		int mc = zkrtspc_media_cnt(c);		// 返回几路media
		for (int i = 0; i < mc; i++) {
			zkrtspc_media_info (c, i, &info);	// 获取媒体信息
			zkrtspc_media_add_cb(c, i, media_callback, opaque); // 注册媒体接收回调函数
		}

		zkrtspc_play(c);			// 启动传输

		....

		zkrtspc_stop(c);			// 停止所有媒体传输

		zkrtspc_close(c);			// 释放 c 对应资源

 */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct zkrtspclient_t zkrtspclient_t;

/* 媒体属性，都从 sdp 中获取 */
typedef struct zkrtspclient_media_info
{
	const char *media_type;		// "audio", "video", "....
	const char *codec_name;		// "h264", "mpeg4-generic....
	const char *fmtp_mode;		// AAC-hbr ...
	const char *fmtp_config;	// 1290...
	int clock_rate;
} zkrtspclient_media_info;

/** 媒体数据回调 
		@param opaque: 
		@param info: 数据属性
		@param data: 媒体数据指针
		@param len: 媒体数据长度
		@param stamp: 时间戳，秒

		@warning: 如果 len == 0，说明 media 结束！
 */
typedef void (*PFN_zkrtspclient_media_cb)(void *opaque, const zkrtspclient_media_info *info, const void *data, int len, double stamp);

/** 获取一个对应 rtsp url 的 client 实例，将阻塞直到 DESCRIBE 返回 */
zkrtspclient_t *zkrtspc_open(const char *url);
void zkrtspc_close (zkrtspclient_t *c);

/** 返回 包含 stream 的数目 */
int zkrtspc_media_cnt(zkrtspclient_t *c);

/** 返回 stream index 的属性 */
int zkrtspc_media_info(zkrtspclient_t *c, int index, zkrtspclient_media_info *info);

/** 注册媒体回调 */
int zkrtspc_media_set_cb(zkrtspclient_t *c, int index, PFN_zkrtspclient_media_cb proc, void *opaque);

/** 启动传输实例 */
int zkrtspc_play(zkrtspclient_t *c);

#ifdef __cplusplus
}
#endif

#endif /** zkrtspclient.h */
