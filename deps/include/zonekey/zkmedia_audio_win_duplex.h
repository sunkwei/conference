#ifndef _zkmedia_audio_win_duplex__hh
#define _zkmedia_audio_win_duplex__hh

#include "zkmedia_audio_win_common.h"

typedef struct zkMediaAudioDuplex_t zkMediaAudioDuplex_t;

zkMediaAudioDuplex_t *zkma_open_duplex(zkMediaAudioConfig *cfg, 
				PFN_zkMediaAudioCallback capture_cb, void *capture_opaque,
				PFN_zkMediaAudioCallback playback_cb, void *playback_opaque);
void zkma_close_duplex(zkMediaAudioDuplex_t *c);

#endif //
