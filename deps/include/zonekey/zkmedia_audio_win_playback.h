#ifndef _zkmedia_audio_win_playback__hh
#define _zkmedia_audio_win_playback__hh

#include "zkmedia_audio_win_common.h"

typedef struct zkMediaAudioPlayback_t zkMediaAudioPlayback_t;

zkMediaAudioPlayback_t *zkma_open_playback(zkMediaAudioConfig *cfg, PFN_zkMediaAudioCallback proc, void *opaque);
void zkma_close_playback(zkMediaAudioPlayback_t *ply);

#endif
