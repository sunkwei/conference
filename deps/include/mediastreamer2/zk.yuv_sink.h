#ifndef __zonekey_yuv_sink__hh
#define __zonekey_yuv_sink__hh

#include "allfilters.h"
#include "msfilter.h"

#ifdef __cplusplus
extern "C" {
#endif // c++

/** 为了方便应用程序从 yuv sink 中读取每一帧数据
    可以设置一个回调函数，当有完整 yuv 图像时，将调用之
 */
typedef struct ZonekeyYUVSinkCallbackParam
{
	void *ctx;		// 将用于调用 push 函数.
	void (*push)(void *ctx, int width, int height, unsigned char *data[4], int stride[4]);	// 回掉函数，当收到了完整 yuv420p 数据后，将调用该函数指针.
} ZonekeyYUVSinkCallbackParam;

// filter id
#define ZONEKEY_ID_YUV_SINK (MSFilterInterfaceBegin+11)

// method id
#define ZONEKEY_METHOD_YUV_SINK_SET_CALLBACK_PARAM		MS_FILTER_METHOD(ZONEKEY_ID_YUV_SINK, 1, ZonekeyYUVSinkCallbackParam)
#define ZONEKEY_METHOD_YUV_SINK_SET_IMAGE_WIDTH			MS_FILTER_METHOD(ZONEKEY_ID_YUV_SINK, 2, int)
#define ZONEKEY_METHOD_YUV_SINK_SET_IMAGE_HEIGHT		MS_FILTER_METHOD(ZONEKEY_ID_YUV_SINK, 3, int)

// 注册filter
MS2_PUBLIC void zonekey_yuv_sink_register();

#ifdef __cplusplus
}
#endif // c++

#endif /* zk.yuv_sink.h */
