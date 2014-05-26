#ifndef _audiostream_gips__hh
#define _audiostream_gips__hh

#include <mediastreamer2/msfilter.h>

#ifdef __cplusplus
extern "C" {
#endif // c++

typedef struct AudioStreamGips AudioStreamGips;
typedef void (*as_gips_on_audio_data)(void *audio_data, int size, int len, void *userdata);
/** 新建实例，指定自己的 rtpport, rtcpport
 */
MS2_PUBLIC AudioStreamGips *audio_stream_gips_new(int rtpport, int rtcpport);

/** 启动一个到远端（ip:rtpport:rtcpport) 的音频回话
 */
MS2_PUBLIC int audio_stream_gips_start(AudioStreamGips *asg, int payload_type, const char *remote_ip, int remote_rtp, int remote_rtcp,	int jitter_comp);

/** 停止
 */
MS2_PUBLIC int audio_stream_gips_stop(AudioStreamGips *asg);

MS2_PUBLIC int audio_stream_record_start(AudioStreamGips *asg, as_gips_on_audio_data, void *userdata);
MS2_PUBLIC int audio_stream_record_stop(AudioStreamGips *asg);
/** 释放
 */
MS2_PUBLIC void audio_stream_gips_destroy(AudioStreamGips *asg);

#ifdef __cplusplus
}
#endif // c++

#endif // audiostream_gips.h
