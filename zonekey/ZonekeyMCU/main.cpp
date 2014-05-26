#include <stdio.h>
#include <stdlib.h>
#include "log.h"
#include "Server.h"
#include <mediastreamer2/mediastream.h>
#include <mediastreamer2/msrtp.h>
#include <mediastreamer2/zk.video.mixer.h>
#include <mediastreamer2/zk.publisher.h>
#include "MixerStream.h"

int main(int argc, char **argv)
{
#ifdef _DEBUG
	putenv("xmpp_domain=qddevsrv.zonekey");
#endif // debug

	log_init();
	log("%s: starting...\n", argv[0]);

	ortp_init();
	ms_init();

	ortp_set_log_level_mask(ORTP_ERROR);

	// “Ù∆µ±ÿ–Î spees£¨ ”∆µ±ÿ–Î h264
	rtp_profile_set_payload(&av_profile, 110, &payload_type_speex_wb);	// 16k
	rtp_profile_set_payload(&av_profile,100,&payload_type_h264);

	// ◊¢≤· zonekey.video.mixer filter, zonekey.publisher filter
	zonekey_video_mixer_register();
	zonekey_publisher_register();

	Server server;
	server.run();

	return 0;
}
