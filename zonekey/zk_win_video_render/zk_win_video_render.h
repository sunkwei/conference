#pragma once

/** 这个库主要为了方便给 c# 使用

		c# 提供一个窗口句柄，提供 server_ip, server_rtp, server_rtcp 参数，收到数据口，将在指定的窗口中显示视频
 */

#ifdef __cplusplus
extern "C" {
#endif // c++

typedef struct zk_vr_t zk_vr_t;

// 必须首先调用！
void zkvr_init();

// 获取一个实例
zk_vr_t *zkvr_open(void *hwnd, const char *server_ip, int server_rtp_port, int server_rtcp_port);

// 释放
void zkvr_close(zk_vr_t *ins);

#ifdef __cplusplus
}
#endif // c++
