#ifndef __zonekey_h264_source__hh
#define __zonekey_h264_source__hh

#include "allfilters.h"
#include "msfilter.h"

#ifdef __cplusplus
extern "C" {
#endif // c++

typedef struct ZonekeyH264SourceWriterParam
{
	void *ctx; /** 用于调用 write */

	/** 外部应用调用，提供 h264 帧数据，data 必须为完整的frame
		stamp 为 秒
	 */
	int (*write)(void *ctx, const void *data, int len, double stamp);
} ZonekeyH264SourceWriterParam;

// 呵呵，离得远点，防止冲突
#define ZONEKEY_ID_H264_SOURCE (MSFilterInterfaceBegin+10)

#define ZONEKEY_METHOD_H264_SOURCE_GET_WRITER_PARAM MS_FILTER_METHOD(ZONEKEY_ID_H264_SOURCE, 1, ZonekeyH264SourceWriterParam)

// 注册filter
MS2_PUBLIC void zonekey_h264_source_register();

#ifdef __cplusplus
}
#endif // c++

#endif /* zk.h264.source.h */
