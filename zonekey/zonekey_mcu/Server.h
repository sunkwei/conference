#pragma once

#include <zonekey/zkrobot_mse_ex.h>
#include <vector>
#include <cc++/thread.h>
#include "Conference.h"

struct GlobalConfig
{
	unsigned int cap_max;	// 通知进行的会议的数目

	GlobalConfig()
	{
		cap_max = -1;
	}
};

extern GlobalConfig *_gc;

// 提供 mse 控制接口
class Server
{
public:
	Server(void);
	~Server(void);

	void run(int *quit);

private:
	// 会议列表
	typedef std::vector<Conference*> CONFERENCES;
	CONFERENCES conferences_;
	ost::Mutex cs_conferences_;

	// 用于测试的一个会议 :)
	//Conference *conference_;

	zkrobot_mse_t *robot_;	// 注册为 zonekey.mse 服务，用于接受控制命令

	static void callback_login(zkrobot_mse_t * mse_ins, int is_successful, void *userdata);
	static void callback_command(zkrobot_mse_t *mse_ins, const char *remote_jid, const char *str_cmd, const char *str_option, 
	                int is_delay, void *userdata, void *token);

	int next_id_;

private:
	//Conference *test_conf_free_;	// 测试用自由会议
	//Conference *test_conf_director_;


	// 会议命令
	int create_conference(KVS &params, KVS &results);
	int destroy_conference(KVS &params, KVS &results);
	int list_conferences(KVS &params, KVS &results);
	int info_conference(KVS &params, KVS &results);

	// 注册会议通知，当会议 stream/source 发生变化时，将返回 reg_notify 的 response
	int reg_notify(const char *from_jid, void *uac_token, KVS &params, KVS &results);

	// 设置会议参数
	int set_params(KVS &params, KVS &results);
	int get_params(KVS &params, KVS &results);

	// 自由会议
	int fc_add_source(KVS &params, KVS &results);
	int fc_del_source(KVS &params, KVS &results);
	int fc_list_sources(KVS &params, KVS &results);
	int fc_add_sink(KVS &params, KVS &results);
	int fc_del_sink(KVS &params, KVS &results);

	// 导播会议
	int dc_add_stream(KVS &params, KVS &results, bool publisher);
	int dc_del_stream(KVS &params, KVS &results);
	int dc_list_streams(KVS &params, KVS &results);
	int dc_add_source(KVS &params, KVS &results);
	int dc_del_source(KVS &params, KVS &results);
	int dc_add_sink(KVS &params, KVS &results);
	int dc_del_sink(KVS &params, KVS &results);
	int dc_get_all_videos(KVS &params, KVS &results);
	int dc_vs_exchange_position(KVS &params, KVS &results);	// 交换两个 video stream 的位置

	// 获取 cpu/mem/network 占用情况
	int get_sys_info(double *cpu, double *mem, double *net_recv, double *net_send);
};
