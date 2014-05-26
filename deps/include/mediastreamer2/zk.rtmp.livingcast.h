/** zonekey mediastreamer2 filter: 用于将音视频(h264+aac)发布到 FMS 服务器

		提供两个 input pin，0 用于 aac，1 用于 h264
		启动之前，必须提供 URL (rtmp://192.168.12.201/livepkgr/steam4123?adbe-live-event=liveevent）


		RTP_RECV (h264) ------------\
									 --------> rtmp livingcast 
		RTP_RECV (AAC with adts) ---/
 */

#ifndef _zonekey_rtmp_livingcast__hh
#define _zonekey_rtmp_livingcast__hh

#include "allfilters.h"
#include "msfilter.h"

#ifdef __cplusplus
extern "C" {
#endif // c++

#define ZONEKEY_ID_RTMP_LIVINGCAST (MSFilterInterfaceBegin+20)
#define ZONEKEY_METHOD_RTMP_LIVINGCAST_SET_URL MS_FILTER_METHOD(ZONEKEY_ID_RTMP_LIVINGCAST, 1, const char*)

MS2_PUBLIC void zonekey_rtmp_livingcast_register();

#ifdef __cplusplus
}
#endif // c++

#endif // zk.rtmp.livingcast.h
