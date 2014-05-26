#pragma once

// 为了方便 c# 调用，在 c# 代码收到 OrtpEvent 之后，交给 rtcp_hlp_xx 处理

#ifdef __cplusplus
extern "C" {
#endif // c++

typedef struct rtcp_hlp_t rtcp_hlp_t;
typedef struct rtcp_hlp_pkg_t rtcp_hlp_pkg_t;
typedef struct rtcp_hlp_report_block_t rtcp_hlp_report_block_t;

// c# 程序收到 rtcp 通知后，需要首先调用这个方法，然后，从继续获取信息....
rtcp_hlp_t *rtcp_hlp_open(void *evt);
void rtcp_hlp_close(rtcp_hlp_t *ctx);

// 返回下一个 rtcp 包，对应每个 rtcp_hlp_t 应该循环调用 next，直到返回 0
rtcp_hlp_pkg_t *rtcp_hlp_next_pkg(rtcp_hlp_t *ctx);
int rtcp_hlp_pkg_is_sr(rtcp_hlp_pkg_t *pkg);
int rtcp_hlp_pkg_is_rr(rtcp_hlp_pkg_t *pkg);

// 返回下一个 report block，循环，直到返回0
rtcp_hlp_report_block_t *rtcp_hlp_report_block_next(rtcp_hlp_pkg_t *pkg);
unsigned int rtcp_hlp_report_block_ssrc(rtcp_hlp_report_block_t *rb);
unsigned int rtcp_hlp_report_block_fraction_lost(rtcp_hlp_report_block_t *rb);
unsigned int rtcp_hlp_report_block_cum_lost(rtcp_hlp_report_block_t *rb);
unsigned int rtcp_hlp_report_block_jitter(rtcp_hlp_report_block_t *rb);

#ifdef __cplusplus
}
#endif // c++
