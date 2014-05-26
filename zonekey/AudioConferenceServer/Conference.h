#pragma once

#include <mediastreamer2/mediastream.h>
#include <mediastreamer2/msconference.h>
#include <cc++/thread.h>
#include <map>
#include <string>

/** 封装 mediaStream2 中的 audioconference 接口. 
 */
class Conference
{
public:
	Conference(int audio_rate);	// 会议使用的采样率. 
	virtual ~Conference(void);

	/** 新增一个成员，成功返回这一端的 local port，失败返回 -1

		 @param pt: 目前总使用 111, speex wb

	 */
	int addMember(const char *ip, int port, int pt);

	/** 删除一个成员，成功返回 0
	 */
	int delMember(const char *ip, int port, int pt);

private:
	struct Who
	{
		std::string ip;
		int port;
		int pt;

		int local_port;	// 本地. 

		AudioStream *as;
		MSAudioEndpoint *ae;

		// 为了支持 map 的 sort.
		bool operator < (const Who &who) const
		{
			return ip < who.ip || ((ip == who.ip) && (port < who.port));
		}
	};

	MSAudioConference *ac_;
	MSAudioConferenceParams acp_;
	typedef std::map<struct Who, AudioStream*> STREAMS;
	STREAMS streams_;		// 对应member的 AudioStream 实例指针. 
	ost::Mutex cs_streams_;
};
