#include "Conference.h"
#include <assert.h>

Conference::Conference(ConferenceMode mode)
	: mode_(mode)
	, mixer_(0)
{
	if (mode == CM_DIRCETOR) {
		mixer_ = new ZonekeyVideoMixer;
	}
}

Conference::~Conference(void)
{
	if (mixer_)
		delete mixer_;
}

MemberStream *Conference::add_member_stream(const char *client_ip, int client_port)
{
	if (mode_ == CM_DIRCETOR) {
		assert(mixer_);

		return mixer_->addStream(client_ip, client_port);
	}
	else {
		// TODO: 自由模式
	}

	return 0;
}

MemberStream *Conference::get_member_stream(int id)
{
	if (mode_ == CM_DIRCETOR)
		return mixer_->getStreamFromID(id);
	else {
		// TODO: 自由模式
		return 0;
	}
}

void Conference::del_member_stream(int id)
{
	if (mode_ == CM_DIRCETOR) {
		assert(mixer_);

		mixer_->delStream(mixer_->getStreamFromID(id));
	}
	else {
		// TODO: 自由模式
	}
}
