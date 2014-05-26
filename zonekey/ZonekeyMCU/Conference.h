#pragma once

#include "WorkerThread.h"
#include "MemberStream.h"
#include "MixerStream.h"

// 会议模式，主席，自由...
enum ConferenceMode
{
	CM_DIRCETOR,	// 导播模式：导播模式中，所有成员的视频由导播决定混合，压缩，发给每个成员
	CM_FREE,		// 自由模式：每个成员自行选择希望看的内容
};

/** 对应一个会议，
 */
class Conference
{
	ConferenceMode mode_;
	ZonekeyVideoMixer *mixer_;	// 用于主席模式

public:
	Conference(ConferenceMode mode);
	virtual ~Conference(void);

	ConferenceMode mode() const { return mode_; }

	// 添加一个会议成员
	MemberStream *add_member_stream(const char *client_ip, int client_port);

	// 根据 MemberStream::id() 找到对应的 MemberStream
	MemberStream *get_member_stream(int id);

	// 释放会议成员
	void del_member_stream(int id);

private:

};
