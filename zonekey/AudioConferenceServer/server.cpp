#include <stdio.h>
#include <stdlib.h>
#include <cc++/thread.h>
#include <string.h>
#include "server.h"
#include <mediastreamer2/mediastream.h>
#include <cc++/network.h>

static const char *_get_myip()
{
	static std::vector<std::string> ips;
	std::vector<ost::NetworkDeviceInfo> nds;

	if (!ips.empty()) {
		return ips[0].c_str();
	}

	enumNetworkDevices(nds);
	std::vector<ost::NetworkDeviceInfo>::const_iterator it;
	for (it = nds.begin(); it != nds.end(); ++it) {
		size_t cnt = it->address().getAddressCount();
		for (size_t i = 0; i < cnt; i++) {
			in_addr addr = it->address().getAddress(i);
			ips.push_back(inet_ntoa(addr));

			fprintf(stdout, "[Server]: find local ip=%s\n", inet_ntoa(addr));
		}
	}

	return ips[0].c_str();
}

Server::Server()
{
	_get_myip();

	ms_base_init();
	ms_voip_init();

	// 支持的语音类型. 
	rtp_profile_set_payload(&av_profile, 110, &payload_type_speex_nb);
	rtp_profile_set_payload(&av_profile, 111, &payload_type_speex_wb);

	mse_ = 0;

	const char *domain = MSE_DOMAIN;
	char *domain_env = getenv("xmpp_domain");
	if (domain_env)
		domain = domain_env;

	fprintf(stderr, "[Server] using xmpp domain = '%s'\n", domain);
	zkrbt_mse_init(domain);

	conference_ = new Conference(16000);
}

Server::~Server()
{
	delete conference_;

	zkrbt_mse_shutdown();

	ms_voip_exit();
	ms_base_exit();
}

static bool _quit = false;

#ifdef WIN32
static BOOL WINAPI handler_ctrl_c(DWORD type)
{
	_quit = true;
	return TRUE;
}
#else
#endif // win32

void Server::run()
{
	// 注册为mse服务.
	zkrobot_mse desc;
	desc.catalog = ZKROBOT_SERVICE;
	strcpy(desc.id, MSE_SERVICE_NAME);
	strcpy(desc.type, MSE_SERVICE_TYPE);
	strcpy(desc.mse_ins.service.url, "");

	fprintf(stdout, "[Server] to login mse as id='%s', type='%s'\n", desc.id, desc.type);

	zk_mse_cbs cbs = { _mse_command, _mse_login };

	mse_ = zkrbt_mse_new(&desc, &cbs, this);

	// 注册 ctrl+c.
#ifdef WIN32
	SetConsoleCtrlHandler(handler_ctrl_c, 1);
#else
#endif // 

	// 等待结束.
	while (!_quit) {
		ost::Thread::sleep(1000);
	}

	// 注销 mse 服务.
	zkrbt_mse_destroy(mse_);
}

void Server::_mse_command(zkrobot_mse_t *mse_ins, const char *remote_jid, const char *str_cmd, const char *str_option, 
	                int is_delay, void *userdata, void *token)
{
	/** 支持的命令
			1. 创建会议；
			2. 删除会议；
			3. 返回会议列表：
			4. 查询会议描述

			5. 申请加入会议：
			6. 申请退出会议：

			... 
	 */

#define STR_CONFERENCE_CREATE "conference.create"
#define STR_CONFERENCE_DESTROY "conference.destory"
#define STR_CONFERENCE_LIST "conference.list"
#define STR_CONFERENCE_DESCR "conference.describe"
#define STR_MEMBER_JOIN "member.join"
#define STR_MEMBER_EXIT "member.exit"

	Server *server = (Server*)userdata;
	if (!strcmp(STR_MEMBER_JOIN, str_cmd)) {
		/** cmd_options: ip=xxx|port=xxx 入参为client的ip+port
			result_options: ip=xxx|port=xxx 返回值为 server 的 ip+port
		 */
		char ip[64];
		int port = 0;
		int rc = sscanf(str_option, "ip=%63[^|] | port=%d", ip, &port);
		if (rc == 2) {
			port = server->conference_->addMember(ip, port, 111);

			char result[128];
			snprintf(result, sizeof(result), "ip=%s|port=%d", _get_myip(), port);
			zkrbt_mse_respond(token, "ok", result);
		}
		else {
			zkrbt_mse_respond(token, "err", "request params format err: ip=xxx|port=xxx");
		}
	}
	else if (!strcmp(STR_MEMBER_EXIT, str_cmd)) {
		/** cmd_options: ip=xxx|port=xxx 入参为client的ip+port
			result_options：没有使用.
		 */
		char ip[64];
		int port = 0;
		int rc = sscanf(str_option, "ip=%63[^|] | port=%d", ip, &port);
		if (rc == 2) {
			server->conference_->delMember(ip, port, 111);

			zkrbt_mse_respond(token, "ok", "");
		}
		else {
			zkrbt_mse_respond(token, "err", "request params format err: ip=xxx|port=xxx");
		}
	}
}

void Server::_mse_login(zkrobot_mse_t * mse_ins, int is_successful, void *userdata)
{
	fprintf(stderr, "[Server]: mse login success = %d\n", is_successful);
}
