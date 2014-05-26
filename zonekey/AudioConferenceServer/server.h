/** server.h 基于 zkrobot_ex，使用服务类型 acs
 */

#pragma once

#include <zonekey/zkrobot_mse_ex.h>
#include <mediastreamer2/mediastream.h>
#include "Conference.h"

#define MSE_SERVICE_TYPE "acs"

// FIXME: 这个应该使用主机的mac地址更好些.
#define MSE_SERVICE_NAME "acs"

// 将被环境变量 xmpp_domain 覆盖.
#define MSE_DOMAIN "app.zonekey.com.cn"

class Server
{
public:
	Server();
	~Server();

	// 执行并阻塞，直到收到 ctrl+c 退出. 
	void run();

private:
	zkrobot_mse_t *mse_;

	// 目前仅仅一个 conference 吧.
	Conference *conference_;

	static void _mse_command(zkrobot_mse_t *mse_ins, const char *remote_jid, const char *str_cmd, const char *str_option, 
	                int is_delay, void *userdata, void *token);
	static void _mse_login(zkrobot_mse_t * mse_ins, int is_successful, void *userdata);
};
