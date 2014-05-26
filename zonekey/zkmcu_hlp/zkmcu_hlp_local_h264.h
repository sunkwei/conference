#pragma once

/** 相当于结合 zkmcu_hlp_h264_render 和 zkmcu_hlp_h264_source
 */

#ifdef __cplusplus
extern "C" {
#endif // c++

typedef struct zkmcu_hlp_local_h264_t zkmcu_hlp_local_h264_t;

zkmcu_hlp_local_h264_t *zkmcu_hlp_local_h264_open(void *wnd);
void zkmcu_hlp_local_h264_close(zkmcu_hlp_local_h264_t *ctx);
int zkmcu_hlp_local_h264_set(zkmcu_hlp_local_h264_t *obj, const void *data, int len, double stamp);

#ifdef __cplusplus
}
#endif // c++
