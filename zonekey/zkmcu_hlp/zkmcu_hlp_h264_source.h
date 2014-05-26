/** 主要设计给 c# 使用，用于直接将数据发送到 mcu 指定的 server ip, port，数据格式使用
 */

#ifndef _zkmcu_hlp_h264_source__hh
#define _zkmcu_hlp_h264_source__hh

#ifdef __cplusplus
extern "C" {
#endif 

typedef struct zkmcu_hlp_h264_source_t zkmcu_hlp_h264_source_t ;

zkmcu_hlp_h264_source_t *zkmcu_hlp_h264_source_open(const char *server_ip, int server_rtp_port, int server_rtcp_port);
void zkmcu_hlp_h264_source_close(zkmcu_hlp_h264_source_t *obj);
int zkmcu_hlp_h264_source_send(zkmcu_hlp_h264_source_t *obj, const void *data, int len, double stamp);

typedef struct rtcp_report_block
{
    uint32_t ssrc;
    uint32_t fl_cnpl;/*fraction lost + cumulative number of packet lost*/
    uint32_t ext_high_seq_num_rec; /*extended highest sequence number received */
    uint32_t interarrival_jitter;
    uint32_t lsr; /*last SR */
    uint32_t delay_snc_last_sr; /*delay since last sr*/
} rtcp_report_block_t;

// FIXME: 这里认为 rtp session 中只有一个 ssrc
int zkmcu_hlp_h264_source_get_report_block(zkmcu_hlp_h264_source_t *obj, rtcp_report_block_t *rb);

#ifdef __cplusplus
}
#endif

#endif // 
