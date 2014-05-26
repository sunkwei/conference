#include "Server.h"
#include <cc++/thread.h>
#include "util.h"
#include "log.h"

Server::Server(void)
{
	char *domain = getenv("xmpp_domain");
	if (!domain)
		domain = "app.zonekey.com.cn";

	zkrbt_mse_init(domain);

	zkrobot_mse mse_desc;
	sprintf(mse_desc.id, "mse_s_000000000000_mcu_0");
	mse_desc.catalog = ZKROBOT_SERVICE;
	sprintf(mse_desc.mse_ins.service.url, "mcu://%s", util_get_myip());
	sprintf(mse_desc.type, "mcu");
	
	zk_mse_cbs cbs = { callback_command, callback_login };
	robot_ = zkrbt_mse_new(&mse_desc, &cbs, this);

	// TEST:
	conference_ = new Conference(CM_DIRCETOR);
}


Server::~Server(void)
{
	if (robot_)
		zkrbt_mse_destroy(robot_);
	zkrbt_mse_shutdown();
}

void Server::run()
{
	while (1)
		ost::Thread::sleep(1000);
}

MemberStream *Server::cmd_conference_add_member(const char *ip, int port, int x, int y, int width, int height, int alpha)
{
	log("[Server] %s: ip=%s, port=%d, [%d, %d, %d, %d], alpha=%d\n", __FUNCTION__, ip, port, x, y, width, height, alpha);

	// 必定为主席模式
	MemberStream *s = conference_->add_member_stream(ip, port);
	s->set_params("ch_desc", 5, x, y, width, height, alpha);

	return s;
}

void Server::callback_login(zkrobot_mse_t * mse_ins, int is_successful, void *userdata)
{
	log("[Server]: %s: success=%d\n", __FUNCTION__, is_successful);
}

void Server::callback_command(zkrobot_mse_t *mse_ins, const char *remote_jid, const char *str_cmd, const char *str_option, 
	                int is_delay, void *userdata, void *token)
{
	Server *This = (Server*)userdata;

	/** 这里先定义几个命令用于测试
	 */
	if (!strcmp(str_cmd, "test.c.add_member")) {
		/** 为会议增加成员：
				options: ip=172.16.1.103&port=33124&x=0&y=0&width=400&height=300&alpha=255
				result_options: cid=0&ip=172.16.1.104&port=55000
		 */
		if (str_option) {
			KVS params = util_parse_options(str_option);
			char info[1024];
			if (chk_params(params, info, "ip", "port", "x", "y", "width", "height", "alpha", 0)) {
				MemberStream *s = This->cmd_conference_add_member(params["ip"].c_str(), atoi(params["port"].c_str()), 
					atoi(params["x"].c_str()), atoi(params["y"].c_str()), atoi(params["width"].c_str()), atoi(params["height"].c_str()),
					atoi(params["alpha"].c_str()));
				if (s) {
					sprintf(info, "cid=%d&ip=%s&port=%d", s->id(), s->get_server_ip(), s->get_server_port());
					zkrbt_mse_respond(token, "ok", info);
				}
				else {
					zkrbt_mse_respond(token, "err", "can't create stream :(");
				}
			}
			else {
				zkrbt_mse_respond(token, "err", info);
			}
		}
		else {
			zkrbt_mse_respond(token, "err", "need options");
		}
	}
	else if (!strcmp(str_cmd, "test.c.del_member")) {
		/** 删除会员
				options: id=0
				result_options: <null>
		 */
		if (str_option) {
			KVS params = util_parse_options(str_option);
			char info[1024];
			if (chk_params(params, info, "id", 0)) {
				This->conference_->del_member_stream(atoi(params["id"].c_str()));
				zkrbt_mse_respond(token, "ok", "");
			}
			else {
				zkrbt_mse_respond(token, "err", info);
			}
		}
		else {
			zkrbt_mse_respond(token, "err", "need options");
		}
	}
	else if (!strcmp(str_cmd, "test.c.set_member")) {
		/** 调整一路视频的属性
				options: id=0&x=0&y=0&width=100&height=400&alpha=200
				result_options: null
		 */
		if (str_option) {
			KVS params = util_parse_options(str_option);
			char info[1024];
			if (chk_params(params, info, "id", "x", "y", "width", "height", "alpha", 0)) {
				MemberStream *s = This->conference_->get_member_stream(atoi(params["id"].c_str()));
				if (s) {
					s->set_params("ch_desc", 5, atoi(params["x"].c_str()), atoi(params["y"].c_str()), 
						atoi(params["width"].c_str()), atoi(params["height"].c_str()), atoi(params["alpha"].c_str()));
					zkrbt_mse_respond(token, "ok", "");
				}
				else {
					zkrbt_mse_respond(token, "err", "id NOT found");
				}
			}
			else {
				zkrbt_mse_respond(token, "err", info);
			}
		}
		else {
			zkrbt_mse_respond(token, "err", "need options");
		}
	}
	else if (!strcmp(str_cmd, "test.c.zorder")) {
		/** zorder 调整:
				options: id=0&mode=['up'|'down'|'top'|'bottom']
				result_options: ok
		 */
		if (str_option) {
			KVS params = util_parse_options(str_option);
			char info[1024];
			if (chk_params(params, info, "id", "mode", 0)) {
				MemberStream *s = This->conference_->get_member_stream(atoi(params["id"].c_str()));
				if (s) {
					s->set_params("ch_zorder", 1, params["mode"].c_str());
				}
				else {
					zkrbt_mse_respond(token, "err", "id NOT found");
				}
			}
			else {
				zkrbt_mse_respond(token, "err", info);
			}
		}
		else {
			zkrbt_mse_respond(token, "err", "need options");
		}
	}
}
