/**  render filter, 一般用于链接在 h264 rtp recver 上，将收到的 rtp 打包为 h264 frame(slice)，然后输出
 */

#ifndef _zonekey_h264_dump__hh
#define _zonekey_h264_dump__hh

#include "mediastream.h"
#include "allfilters.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ZONEKEY_ID_H264_DUMP (MSFilterInterfaceBegin+15)

/** 当内部收到完整的 frame 后，回调
 */
typedef struct ZonekeyH264DumpParam
{
	void *opaque;
	void (*push)(void *opaque, const void *data, int len, double stamp, int key);
} ZonekeyH264DumpParam;

// method id
#define ZONEKEY_METHOD_H264_DUMP_SET_CALLBACK_PARAM		MS_FILTER_METHOD(ZONEKEY_ID_H264_DUMP, 1, ZonekeyH264DumpParam)

MS2_PUBLIC void zonekey_h264_dump_register();

#ifdef __cplusplus
}
#endif

#endif 
