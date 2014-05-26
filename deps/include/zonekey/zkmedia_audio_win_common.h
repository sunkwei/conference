#ifndef _zk_media_audio_win_common__hh
#define _zk_media_audio_win_common__hh

#ifdef __cplusplus
extern "C" {
#endif // c++

// 配置参数. 
typedef struct zkMediaAudioConfig
{
	int channels;		// 
	int bits;			// 采样大小，16bits...
	int rate;			// 采样率
	int frame_size;		// 回调通知的一段大小，字节大小. 
} zkMediaAudioConfig;

// 回调函数原型, 当数据有效时，对于 capture，说明数据已经获得，对于 player，说明需要调用者提供数据. 
// 不得阻塞！！！总是应该尽快返回
typedef void (*PFN_zkMediaAudioCallback)(void *opaque, void *pcm, int len);

#ifdef __cplusplus
}
#endif // c++

#endif 
