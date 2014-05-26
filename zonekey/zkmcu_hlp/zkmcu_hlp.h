// 用于方便 c# 使用 mcu 的媒体功能

#ifndef _zkmcu_hlp__hh
#define _zkmcu_hlp__hh

#ifdef __cplusplus
extern "C" {
#endif

// 必须首先调用！！！
void zkmcu_hlp_init();

#include "zkmcu_hlp_h264_source.h"
#include "zkmcu_hlp_audio_stream.h"
#include "zkmcu_hlp_h264_render.h"
#include "zkmcu_hlp_webcam.h"
#include "zkmcu_hlp_conference.h"
#include "zkmcu_hlp_local_h264.h"
#include "zkmcu_hlp_h264_intel_decoder_render.h"

#ifdef __cplusplus
}
#endif

#endif // 
