#include "Conference.h"

static int _next_port()
{
	static int port = 35000;
	
	port += 2;
	return port;
}

Conference::Conference(int rate)
{
	acp_.samplerate = rate;
	ac_ = ms_audio_conference_new(&acp_);

	fprintf(stdout, "[Conference]: %p: inited, using samplerate=%d!\n", this, rate);
}

Conference::~Conference(void)
{
	if (ac_) {
		ms_audio_conference_destroy(ac_);
		ac_ = 0;
	}

	fprintf(stdout, "[Conference]: %p: shutdown!\n", this);
}

int Conference::addMember(const char *ip, int port, int pt)
{
	for (int i = 0; i < 10; i++) {
		int local_port = _next_port();

		Who who = { ip, port, pt, local_port, 0, 0 };
		who.as = audio_stream_new(who.local_port, who.local_port+1, false);
		int rc = audio_stream_start_with_files(who.as, &av_profile, ip, port, port+1, pt, 50, 0, 0);
		if (rc == 0) {
			ortp_set_log_level_mask(ORTP_FATAL);
			who.ae = ms_audio_endpoint_get_from_stream(who.as, 1);
			ms_audio_conference_add_member(ac_, who.ae);

			ost::MutexLock al(cs_streams_);
			streams_[who] = who.as;
			fprintf(stdout, "[Conference]: %p: addMember for %s:%d.%d OK! local port=%d\n", this, ip, port, pt, who.local_port);
			return who.local_port;
		}
		else {
			fprintf(stderr, "[Conference]: %p: addMember for %s:%d.%d err, local port=%d\n", this, ip, port, pt, who.local_port);
			audio_stream_stop(who.as);
		}
	}

	return -1;
}

int Conference::delMember(const char *ip, int port, int pt)
{
	Who who = { ip, port, pt, 0, 0, 0 };

	ost::MutexLock al(cs_streams_);
	STREAMS::iterator itf = streams_.find(who);
	if (itf != streams_.end()) {
		ms_audio_conference_remove_member(ac_, itf->first.ae);
		ms_audio_endpoint_release_from_stream(itf->first.ae);
		audio_stream_stop(itf->first.as);

		return 0;
	}
	else
		return -1;
}
