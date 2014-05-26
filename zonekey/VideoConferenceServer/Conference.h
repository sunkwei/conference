/** 每个 conference 配合一个 video mixer */

#pragma once

#include <mediastreamer2/mediastream.h>
#include <mediastreamer2/zk.video.mixer.h>

class Conference
{
public:
	Conference(void);
	~Conference(void);

	// 添加成员
	int addMember(const char *ip, int port);
	int delMember(const char *ip, int port);
};
