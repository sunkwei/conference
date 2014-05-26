#ifndef _RECORD_FILTER_HH
#define _RECORD_FILTER_HH

#include <mediastreamer2/msfilter.h>
#include <mediastreamer2/mediastream.h>

extern MSFilterDesc zkrecorder_desc;

#define ZKRECORDER_ID (MSFilterInterfaceBegin + 20)

#define ZKRECORDER_METHOD_SET_CALLBACK MS_FILTER_METHOD(ZKRECORDER_ID, 0, zkrecorder_cb_data)

#endif
