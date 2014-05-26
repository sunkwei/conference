#ifndef __zonekey_video_record__hh
#define __zonekey_video_record__hh

#include "allfilters.h"
#include "msfilter.h"

#ifdef __cplusplus
extern "C" {
#endif // c++

typedef struct ZonekeyRecordCallbackParam
{
	void *userdata;		
	void (*on_data)(unsigned char *data, int len, int is_key, double stamp, void *userdata);
} ZonekeyRecordCallbackParam;
#define ZONEKEY_VIDEO_RECORDER_NAME "zkvideo_recorder"
// filter id
#define ZONEKEY_ID_VIDEO_RECORD (MSFilterInterfaceBegin+22)

// method id
#define ZONEKEY_METHOD_RECORD_SET_CALLBACK_PARAM	MS_FILTER_METHOD(ZONEKEY_ID_VIDEO_RECORD, 1, ZonekeyRecordCallbackParam)

// ×¢²áfilter
MS2_PUBLIC void zonekey_video_record_register();

#ifdef __cplusplus
}
#endif // c++

#endif /* zk.yuv_sink.h */
