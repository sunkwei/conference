#pragma once

#include <zonekey/zkrobot_mse_ex.h>
#include <vector>
#include <cc++/thread.h>
#include "Conference.h"
#include "MixerStream.h"
#include "MemberStream.h"

// 提供 mse 控制接口
class Server
{
public:
	Server(void);
	~Server(void);

	void run();

protected:
	MemberStream *cmd_conference_add_member(const char *remote_ip, int remote_port, int x, int y, int width, int height, int alpha);
	

private:
	// 会议列表
	typedef std::vector<Conference*> CONFERENCES;
	CONFERENCES conferences_;

	// 用于测试的一个会议 :)
	Conference *conference_;

	zkrobot_mse_t *robot_;	// 注册为 zonekey.mse 服务，用于接受控制命令

	static void callback_login(zkrobot_mse_t * mse_ins, int is_successful, void *userdata);
	static void callback_command(zkrobot_mse_t *mse_ins, const char *remote_jid, const char *str_cmd, const char *str_option, 
	                int is_delay, void *userdata, void *token);
};
