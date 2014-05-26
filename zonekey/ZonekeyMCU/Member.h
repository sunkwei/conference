#pragma once

/** 一个会议成员，具有自己的 CNAME，包含一路音频+0到多路视频

		对于 MCU 来说，Member 重点是一组 rtp recver(接收该成员上传的数据）和一组 rtp sender（将混合数据，或者该成员点播的数据发送到该成员）
 */

class Member
{
public:
	Member(void);
	virtual ~Member(void);
};
