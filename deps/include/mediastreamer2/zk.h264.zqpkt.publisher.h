#ifndef _zonekey_h264_zqpkt__hh
#define _zonekey_h264_zqpkt__hh

#include "mediastream.h"
#include "allfilters.h"

#ifdef __cplusplus
extern "C" {
#endif // c++

#define ZONEKEY_ID_ZQPKT_PUBLISHER (MSFilterInterfaceBegin+13)

// 启动的 zq_sender 的端口号
#define ZONEKEY_METHOD_SET_ZQPKT_PUBLISHER_PORT MS_FILTER_METHOD(ZONEKEY_ID_ZQPKT_PUBLISHER, 1, int)

MS2_PUBLIC void zonekey_h264_zqpkt_publisher_register();

#ifdef __cplusplus
}
#endif // c++

#endif 
