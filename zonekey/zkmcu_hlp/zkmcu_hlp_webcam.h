#pragma once

/** 检查是否有本地 webcam，如果有，可以从中获取 h264 帧数据，并便于通过 zkmcu_hlp_h264_source 接口发送出去
 */

#ifndef __cplusplus
extern "C" {
#endif // c++

typedef struct zkmcu_hlp_webcam_t zkmcu_hlp_webcam_t;

// 返回是否存在可用的 webcam
int zkmcu_hlp_webcam_exist();

// 打开设备
zkmcu_hlp_webcam_t *zkmcu_hlp_webcam_open(int width, int height, double fps, int kbitrate);

// 从设备中取出 h264 数据，阻塞模式
int zkmcu_hlp_webcam_get_h264(zkmcu_hlp_webcam_t *ctx, const void **data, double *pts);

// 关闭设备
void zkmcu_hlp_webcam_close(zkmcu_hlp_webcam_t *ctx);

#ifndef __cplusplus
}
#endif // c++
