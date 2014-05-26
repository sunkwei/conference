#ifndef _zonekey_audio_mixer__hh
#define _zonekey_audio_mixer__hh

/** 类似 msaudiomixer filter，但是比其多出一路 output，用于输出到 publisher，方便点播

	outputs[0..ZONEKEY_AUDIO_MIXER_PREVIEW_PIN-1] 用于输出到会议参与者，没路都不同，因为需要删除自己的声音。

	注意：要求 pcm 格式为 1ch, 16bits, 16000Hz

 */

#include "allfilters.h"
#include "mediastream.h"

#ifdef __cplusplus
extern "C" {
#endif // c++

#define ZONEKEY_AUDIO_MIXER_MAX_CHANNELS 20
#define ZONEKEY_ID_AUDIO_MIXER (MSFilterInterfaceBegin+14)

// 输出所有混合
#define ZONEKEY_AUDIO_MIXER_PREVIEW_PIN (ZONEKEY_AUDIO_MIXER_MAX_CHANNELS-1)

MS2_PUBLIC void zonekey_audio_mixer_register();
MS2_PUBLIC void zonekey_audio_mixer_set_log_handler(void (*func)(const char *, ...));

#ifdef __cplusplus
}
#endif // c++

#endif // zk.audio.mixer.h
