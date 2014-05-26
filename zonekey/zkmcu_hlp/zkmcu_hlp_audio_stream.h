#ifndef _zkmcu_hlp_audio_stream__hh
#define _zkmcu_hlp_audio_stream__hh

#ifdef __cplusplus
extern "C" {
#endif

typedef struct zkmcu_hlp_audio_stream_t zkmcu_hlp_audio_stream_t;

zkmcu_hlp_audio_stream_t *zkmcu_hlp_audio_stream_open(int port, const char *server, int rtp_port, int rtcp_port);
void zkmcu_hlp_audio_stream_close(zkmcu_hlp_audio_stream_t *ins);

#ifdef __cplusplus
}
#endif 

#endif
