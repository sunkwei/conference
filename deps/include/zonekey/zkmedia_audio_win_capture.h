#ifndef _zkmedia_audio_win_capture__hh
#define _zkmedia_audio_win_capture__hh

#include "zkmedia_audio_win_common.h"

typedef struct zkMediaAudioCapture_t zkMediaAudioCapture_t;

zkMediaAudioCapture_t *zkma_open_capture(zkMediaAudioConfig *cfg, PFN_zkMediaAudioCallback proc, void *opaque);
void zkma_close_capture(zkMediaAudioCapture_t *cap);


#endif 
