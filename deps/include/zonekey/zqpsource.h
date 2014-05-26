/** 通过 file:// tcp:// http:// 方式接收zqp格式，并demux
*/

#ifndef _zqp_source__hh
#define _zqp_source__hh

#ifdef __cplusplus
extern "C" {
#endif // c++

/** 声明 zqp 结构
*/
struct zq_pkt
{
	int type;	// 1 video, 2 audio, 3 udp padding pkt
	int id;
	unsigned int pts, dts;	// 45khz
	int len;	// 数据长度
	char *ptr;	// 指向数据的指针

	union {
		struct {
			int frametype;	// I P B
			int width, height;
		} video;

		struct {
			int ch, samplerate, samplesize;
		} audio;
	} data;
};
typedef struct zq_pkt zq_pkt;

/** 声明序列参数集属性，从码流中解析
*/
struct zq_sps
{
	unsigned char profile_idc;
	unsigned char level_idc;
	int seq_param_set_id;
	int pic_width;
	int pic_height;
};

/** 声明图像参数集属性，从码流中解析
*/
struct zq_pps
{
};

#ifdef WIN32
#  ifdef MINGW32
#     define ZQPAPI
#  else
#	ifdef LIBZQPSRC_EXPORTS
#		define ZQPAPI __declspec(dllexport)
#	else
#		define ZQPAPI __declspec(dllimport)
#	endif // LIBZQPSRC_EXPORTS
#  endif // mingw32
#else
#	define ZQPAPI
#endif // win32

/** 打开 url，成功返回 > 0，-1 连接 url 失败
*/
ZQPAPI int zqpsrc_open (void **ctx, const char *url);
/** 同 zqpsrc_open() 但是指定本地绑定的ip, 一般用于多网卡支持
 */
ZQPAPI int zqpsrc_open_bindip (void **ctx, const char *url, const char *bindip);

// 关闭实例
ZQPAPI void zqpsrc_close (void *ctx);
// 重新查找同步头 
ZQPAPI void zqpsrc_reset (void *ctx);
// 获取一个完整 zq_pkt
ZQPAPI zq_pkt *zqpsrc_getpkt (void *ctx);
// 释放
ZQPAPI void zqpsrc_freepkt (void *ctx, zq_pkt *pkt);
// 解析 sps，如果没有获取，返回 0
ZQPAPI zq_sps *zqpsrc_parse_sps (void *ctx, zq_pkt *pkt);
// 解析 pps，如果没有获取，返回 0
ZQPAPI zq_pps *zqpsrc_parse_pps (void *ctx, zq_pkt *pkt);

#ifdef __cplusplus
}
#endif // c++

#endif // zqpsource.h
