#include <mediastreamer2/mediastream.h>
#include <ortp/ortp.h>
#include <mediastreamer2/zk.h264.source.h>
#include <mediastreamer2/zk.yuv_sink.h>
#include <mediastreamer2/zk.rtmp.livingcast.h>
#include <mediastreamer2/zk.void.h>
extern "C" {
#	include <mediastreamer2/record_filter.h>
}
#include <cc++/thread.h>

void zkmcu_hlp_init()
{
	static bool _inited = false;
	static ost::Mutex _cs;
	
	ost::MutexLock al(_cs);

	if (!_inited) {
		ms_init();
		ortp_init();

		ortp_set_log_level_mask(ORTP_FATAL);

		zonekey_h264_source_register();
		zonekey_yuv_sink_register();
		zonekey_void_register();
		

		rtp_profile_set_payload(&av_profile, 100, &payload_type_h264);
		rtp_profile_set_payload(&av_profile, 110, &payload_type_speex_wb);
		rtp_profile_set_payload(&av_profile, 102, &payload_type_ilbc);
	}
}
