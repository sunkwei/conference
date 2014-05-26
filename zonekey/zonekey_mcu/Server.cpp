#include "Server.h"
#include <cc++/thread.h>
#include <algorithm>
#include "util.h"
#include "log.h"
#include "FreeConference.h"
#include "DirectorConference.h"
#include "ver.h"
#include "../cJSON/cJSON.h"

Server::Server(void)
{
//	putenv("xmpp_domain=app.zonekey.com.cn");

	const char *domain = getenv("xmpp_domain");
	if (!domain)
		domain = "app.zonekey.com.cn";

	next_id_ = 1;

	zkrbt_mse_init(domain);

	log("[Server]: my ip is '%s'\n", util_get_myip());
	log("[Server]: my mac is '%s'\n", util_get_mymac());

	zkrobot_mse mse_desc;
	sprintf(mse_desc.id, "mse_s_000000000000_mcu_0");
	mse_desc.catalog = ZKROBOT_SERVICE;
	sprintf(mse_desc.mse_ins.service.url, "mcu://%s", util_get_myip());
	sprintf(mse_desc.type, "mcu");
	
	zk_mse_cbs cbs = { callback_command, callback_login };
	robot_ = zkrbt_mse_new(&mse_desc, &cbs, this);

	log("[Server] try login as jid='%s@%s'\n", mse_desc.id, domain);
}

Server::~Server(void)
{
	if (robot_)
		zkrbt_mse_destroy(robot_);
	zkrbt_mse_shutdown();
}

static void func_run_once(Conference *&conf)
{
	conf->run_once();
}

static bool func_idle(Conference *&conf)
{
	bool idle = conf->idle();
	if (idle)
		delete conf;

	return idle;
}

void Server::run(int *quit)
{
	while (!(*quit)) {
		ost::Thread::sleep(100);

		do {
			ost::MutexLock al(cs_conferences_);
			std::for_each(conferences_.begin(), conferences_.end(), func_run_once);
		} while (0);

		do {
			// 检查 conference 是否空闲？空闲意味着需要主动释放 ...
			ost::MutexLock al(cs_conferences_);
			CONFERENCES::iterator nn = std::remove_if(conferences_.begin(), conferences_.end(), func_idle);
			conferences_.erase(nn, conferences_.end());
		} while (0);
	}
}

void Server::callback_login(zkrobot_mse_t * mse_ins, int is_successful, void *userdata)
{
	log("[Server]: %s: success=%d\n", __FUNCTION__, is_successful);
}

int Server::create_conference(KVS &params, KVS &results)
{
	char info[128];
	ost::MutexLock al(cs_conferences_);

	/** 创建会议：必须包含 mode
	 */
	if (!chk_params(params, info, "mode", 0)) {
		results["reason"] = info;
		return -1;
	}

	/** 检查是否还能启动更多
	 */
	if (conferences_.size() >= _gc->cap_max) {
		results["reason"] = "license: too many conferences...";
		return -1;
	}

	Conference *cf = 0;

	if (params["mode"] == "free")
		cf = new FreeConference(next_id_);
	else if (params["mode"] == "director") {
		// 检查是否需要启动直播
		int livingcast = 0;
		const char *v = get_param_value(params, "livingcast");
		if (v && !strcmp(v, "true"))
			livingcast = 1;
		cf = new DirectorConference(next_id_, livingcast);
	}

	if (!cf) {
		snprintf(info, sizeof(info), "unknown mode='%s'", params["mode"].c_str());
		results["reason"] = info;
		return -1;
	}

	if (chk_params(params, info, "desc", 0))
		cf->set_desc(params["desc"].c_str());

	conferences_.push_back(cf);

	snprintf(info, sizeof(info), "%d", next_id_);
	results["cid"] = info;

	next_id_ ++;
	if (next_id_ >= 0x7f000000)
		next_id_ = 1;

	return 0;
}

int Server::destroy_conference(KVS &params, KVS &results)
{
	char info[128];
	ost::MutexLock al(cs_conferences_);

	/** 删除会议，必须包含 cid
	 */
	if (!chk_params(params, info, "cid", 0)) {
		results["reason"] = info;
		return -1;
	}

	int cid = atoi(params["cid"].c_str());

	CONFERENCES::iterator it;
	bool found = false;
	for (it = conferences_.begin(); it != conferences_.end(); ++it) {
		if ((*it)->cid() == cid) {
			delete *it;
			conferences_.erase(it);
			found = true;
			break;
		}
	}

	if (found) {
		snprintf(info, sizeof(info), "cid=%d, released!", cid);
		results["info"] = info;
		return 0;
	}
	else {
		snprintf(info, sizeof(info), "cid=%d, not found!", cid);
		results["reason"] = info;
		return -1;
	}
}

int Server::list_conferences(KVS &params, KVS &results)
{
	ost::MutexLock al(cs_conferences_);

	std::stringstream ss;

	CONFERENCES::const_iterator it;
	for (it = conferences_.begin(); it != conferences_.end(); ++it)
		ss << (*it)->cid() << ",";

	results["cids"] = ss.str();
	return 0;
}

/**
	{ 
		"conference" : {
			"cid": id,
			"desc": "xxx",
			"mode": "free|director"
			"streams": [
				{
					"streamid": id,
					"desc": "xxx",
					"stat": {
						...
						...
					},
					//"sink_ids": [
					//	"sinkid": xx,
					//	"sinkid": xx,
					//	...
					//],
				},
				{
					"streamid": id,
					"desc": "xxxx",
				},
				...
			],
			"sources": [
				// 类似 streams
			],
			"sinks": [
				// 类似 streams ...
			],
		},
	}
*/
#include "Conference.h"
static std::string info_of_conference(Conference *conf)
{
	std::string info = "";
	cJSON *c, *ca;

	cJSON *root = cJSON_CreateObject();
	
	cJSON_AddItemToObject(root, "conference", c = cJSON_CreateObject());
	cJSON_AddNumberToObject(c, "cid", conf->cid());
	cJSON_AddStringToObject(c, "desc", conf->desc());
	cJSON_AddStringToObject(c, "mode", conf->type() == CT_FREE ? "free" : "direct");
	cJSON_AddNumberToObject(c, "uptime", conf->uptime());
	
	// add streams ...
	cJSON_AddItemToObject(c, "streams", ca = cJSON_CreateArray());
	std::vector<StreamDescription> sds = conf->get_stream_descriptions();
	for (std::vector<StreamDescription>::iterator it = sds.begin(); it != sds.end(); ++it) {
		cJSON *o, *s;
		cJSON_AddItemToArray(ca, o = cJSON_CreateObject());

		cJSON_AddNumberToObject(o, "streamid", it->streamid);
		cJSON_AddStringToObject(o, "desc", it->desc.c_str());
		cJSON_AddItemToObject(o, "stat", s = cJSON_CreateObject());
		
		cJSON_AddNumberToObject(s, "bytes_sent", it->stats.sent);
		cJSON_AddNumberToObject(s, "bytes_recv", it->stats.recv);
		cJSON_AddNumberToObject(s, "packet_sent", it->stats.packet_sent);
		cJSON_AddNumberToObject(s, "packet_recv", it->stats.packet_recv);
		cJSON_AddNumberToObject(s, "packet_lost_sent", it->stats.packet_lost_sent);
		cJSON_AddNumberToObject(s, "packet_lost_recv", it->stats.packet_lost_recv);
		cJSON_AddNumberToObject(s, "jitter", it->stats.jitter);
	}

	// add sources ...
	cJSON_AddItemToObject(c, "source", ca = cJSON_CreateArray());
	std::vector<SourceDescription> sds2 = conf->get_source_descriptions();
	for (std::vector<SourceDescription>::iterator it = sds2.begin(); it != sds2.end(); ++it) {
		cJSON *o, *s;
		cJSON_AddItemToArray(ca, o = cJSON_CreateObject());

		cJSON_AddNumberToObject(o, "sid", it->sid);
		cJSON_AddStringToObject(o, "desc", it->desc.c_str());
		cJSON_AddItemToObject(o, "stat", s = cJSON_CreateObject());

		cJSON_AddNumberToObject(s, "recv", it->stats.recv);
		cJSON_AddNumberToObject(s, "hw_recv", it->stats.hw_recv);
		cJSON_AddNumberToObject(s, "recv_packets", it->stats.recv_packets);
		cJSON_AddNumberToObject(s, "lost_packets", it->stats.lost_packets);
	}

	// add sinks ...
	cJSON_AddItemToObject(c, "sinks", ca = cJSON_CreateArray());
	std::vector<SinkDescription> sds3 = conf->get_sink_descriptions();
	for (std::vector<SinkDescription>::iterator it = sds3.begin(); it != sds3.end(); ++it) {
		cJSON *o, *s;
		cJSON_AddItemToArray(ca, o = cJSON_CreateObject());

		cJSON_AddNumberToObject(o, "sinkid", it->sinkid);
		cJSON_AddStringToObject(o, "desc", it->desc);
		cJSON_AddStringToObject(o, "who", it->who);
		cJSON_AddItemToObject(o, "stat", s = cJSON_CreateObject());

		cJSON_AddNumberToObject(s, "sent", it->stats.sent);
		cJSON_AddNumberToObject(s, "packets", it->stats.packets);
		cJSON_AddNumberToObject(s, "packets_lost", it->stats.packets_lost);
		cJSON_AddNumberToObject(s, "jitter", it->stats.jitter);
	}

	char *ptr = cJSON_Print(root);
	info = ptr;
	free(ptr);

	return info;
}

int Server::info_conference(KVS &params, KVS &results)
{
	ost::MutexLock al(cs_conferences_);
	char info[128];

	if (!chk_params(params, info, "cid", 0)) {
		results["reason"] = info;
		return -1;
	}

	int cid = atoi(params["cid"].c_str());
	bool found = false;
	CONFERENCES::iterator it;
	for (it = conferences_.begin(); it != conferences_.end(); ++it) {
		if ((*it)->cid() == cid) {

			//	results["desc"] = (*it)->desc();
			//	results["mode"] = (*it)->type() == CT_FREE ? "free" : "director";


			// TODO: 更多的属性
			// 2013-8-6: sunkw 新增 sources, streams, sinks 的统计信息
			// 返回会议描述, 使用 json 描述

			results["info"] = info_of_conference(*it);

			found = true;
			break;
		}
	}

	if (!found) {
		snprintf(info, sizeof(info), "cid=%d, NOT found", cid);
		results["reason"] = info;
		return -1;
	}

	return 0;
}

int Server::fc_add_source(KVS &params, KVS &results)
{
	char info[128];
	ost::MutexLock al(cs_conferences_);

	if (!chk_params(params, info, "cid", "payload", 0)) {
		results["reason"] = info;
		return -1;
	}

	int cid = atoi(params["cid"].c_str());
	int payload = atoi(params["payload"].c_str());

	if (payload != 100 && payload != 110) {
		snprintf(info, sizeof(info), "payload type '%d' not supported", payload);
		results["reason"] = info;
		return -1;
	}

	Conference *conf = 0;
	CONFERENCES::iterator it;
	for (it = conferences_.begin(); it != conferences_.end(); ++it) {
		if ((*it)->cid() == cid) {
			conf = *it;
			break;
		}
	}

	if (!conf) {
		snprintf(info, sizeof(info), "cid=%d, NOT found!", cid);
		results["reason"] = info;
		return -1;
	}

	return conf->addSource(params, results);
}

int Server::fc_del_source(KVS &params, KVS &results)
{
	char info[128];
	ost::MutexLock al(cs_conferences_);

	if (!chk_params(params, info, "cid", "sid", 0)) {
		results["reason"] = info;
		return -1;
	}

	int cid = atoi(params["cid"].c_str());
	int sid = atoi(params["sid"].c_str());

	Conference *conf = 0;
	CONFERENCES::iterator it;
	for (it = conferences_.begin(); it != conferences_.end(); ++it) {
		if ((*it)->cid() == cid) {
			conf = *it;
			break;
		}
	}
	
	if (!conf) {
		snprintf(info, sizeof(info), "cid=%d, NOT found!", cid);
		results["reason"] = info;
		return -1;
	}

	return conf->delSource(params, results);
}

int Server::fc_list_sources(KVS &params, KVS &results)
{
	char info[128];
	ost::MutexLock al(cs_conferences_);

	if (!chk_params(params, info, "cid", 0)) {
		results["reason"] = info;
		return -1;
	}

	int cid = atoi(params["cid"].c_str());

	Conference *conf = 0;
	CONFERENCES::iterator it;
	for (it = conferences_.begin(); it != conferences_.end(); ++it) {
		if ((*it)->cid() == cid) {
			conf = *it;
			break;
		}
	}

	if (!conf) {
		snprintf(info, sizeof(info), "cid=%d, NOT found!", cid);
		results["reason"] = info;
		return -1;
	}

	std::vector<SourceDescription> list = conf->get_source_descriptions();
	std::stringstream ss;
	std::vector<SourceDescription>::iterator it2;
	for (it2 = list.begin(); it2 != list.end(); ++it) {
		ss << it2->sid << " " << it2->desc << "\n";
	}

	results["sources"] = ss.str();

	return 0;
}

int Server::fc_add_sink(KVS &params, KVS &results)
{
	char info[128];
	ost::MutexLock al(cs_conferences_);

	if (!chk_params(params, info, "cid", "sid", 0)) {
		results["reason"] = info;
		return -1;
	}

	int cid = atoi(params["cid"].c_str());
	int sid = atoi(params["sid"].c_str());

	Conference *conf = 0;

	if (cid != -1) {
		CONFERENCES::iterator it;
		for (it = conferences_.begin(); it != conferences_.end(); ++it) {
			if ((*it)->cid() == cid) {
				conf = *it;
				break;
			}
		}
	}

	if (!conf) {
		snprintf(info, sizeof(info), "cid=%d, NOT found!", cid);
		results["reason"] = info;
		return -1;
	}

	return conf->addSink(params, results);
}

int Server::fc_del_sink(KVS &params, KVS &results)
{
	char info[128];
	ost::MutexLock al(cs_conferences_);

	if (!chk_params(params, info, "cid", "sinkid", 0)) {
		results["reason"] = info;
		return -1;
	}

	int cid = atoi(params["cid"].c_str());
	Conference *conf = 0;
	if (cid == -1)
		conf = 0;
	else {
		CONFERENCES::iterator it;
		for (it = conferences_.begin(); it != conferences_.end(); ++it) {
			if ((*it)->cid() == cid) {
				conf = *it;
				break;
			}
		}
	}

	if (!conf) {
		snprintf(info, sizeof(info), "cid=%d, NOT found!", cid);
		results["reason"] = info;
		return -1;
	}

	return conf->delSink(params, results);
}

int Server::dc_add_stream(KVS &params, KVS &results, bool publisher)
{
	char info[128];
	ost::MutexLock al(cs_conferences_);

	if (!chk_params(params, info, "cid", "payload", 0)) {
		results["reason"] = info;
		return -1;
	}

	int cid = atoi(params["cid"].c_str());
	Conference *conf = 0;
	if (cid == -1)
		conf = 0;
	else {
		CONFERENCES::iterator it;
		for (it = conferences_.begin(); it != conferences_.end(); ++it) {
			if ((*it)->cid() == cid) {
				conf = *it;
				break;
			}
		}
	}

	if (!conf) {
		snprintf(info, sizeof(info), "cid=%d, NOT found!", cid);
		results["reason"] = info;
		return -1;
	}

	return publisher ? conf->addStreamWithPublisher(params, results) : conf->addStream(params, results);
}

int Server::dc_del_stream(KVS &params, KVS &results)
{
	char info[128];
	ost::MutexLock al(cs_conferences_);

	if (!chk_params(params, info, "cid", "streamid", 0)) {
		results["reason"] = info;
		return -1;
	}

	int cid = atoi(params["cid"].c_str());
	Conference *conf = 0;
	if (cid == -1)
		conf = 0;
	else {
		CONFERENCES::iterator it;
		for (it = conferences_.begin(); it != conferences_.end(); ++it) {
			if ((*it)->cid() == cid) {
				conf = *it;
				break;
			}
		}
	}

	if (!conf) {
		snprintf(info, sizeof(info), "cid=%d, NOT found!", cid);
		results["reason"] = info;
		return -1;
	}

	return conf->delStream(params, results);
}

int Server::dc_add_source(KVS &params, KVS &results)
{
	char info[128];
	ost::MutexLock al(cs_conferences_);

	if (!chk_params(params, info, "cid", "payload", 0)) {
		results["reason"] = info;
		return -1;
	}

	int cid = atoi(params["cid"].c_str());
	Conference *conf = 0;
	if (cid == -1)
		conf = 0;
	else {
		CONFERENCES::iterator it;
		for (it = conferences_.begin(); it != conferences_.end(); ++it) {
			if ((*it)->cid() == cid) {
				conf = *it;
				break;
			}
		}
	}

	if (!conf) {
		snprintf(info, sizeof(info), "cid=%d, NOT found!", cid);
		results["reason"] = info;
		return -1;
	}

	return conf->addSource(params, results);
}

int Server::dc_del_source(KVS &params, KVS &results)
{
	char info[128];
	ost::MutexLock al(cs_conferences_);

	if (!chk_params(params, info, "cid", "sid", 0)) {
		results["reason"] = info;
		return -1;
	}

	int cid = atoi(params["cid"].c_str());
	Conference *conf = 0;
	if (cid == -1)
		conf = 0;
	else {
		CONFERENCES::iterator it;
		for (it = conferences_.begin(); it != conferences_.end(); ++it) {
			if ((*it)->cid() == cid) {
				conf = *it;
				break;
			}
		}
	}

	if (!conf) {
		snprintf(info, sizeof(info), "cid=%d, NOT found!", cid);
		results["reason"] = info;
		return -1;
	}

	return conf->delSource(params, results);
}

int Server::dc_list_streams(KVS &params, KVS &results)
{
	char info[128];
	ost::MutexLock al(cs_conferences_);

	if (!chk_params(params, info, "cid", 0)) {
		results["reason"] = info;
		return -1;
	}

	int cid = atoi(params["cid"].c_str());
	Conference *conf = 0;
	CONFERENCES::iterator it;
	for (it = conferences_.begin(); it != conferences_.end(); ++it) {
		if ((*it)->cid() == cid) {
			conf = *it;
			break;
		}
	}

	if (!conf) {
		snprintf(info, sizeof(info), "cid=%d, NOT found!", cid);
		results["reason"] = info;
		return -1;
	}

	std::vector<StreamDescription> descs = conf->get_stream_descriptions();
	std::stringstream ss;
	std::vector<StreamDescription>::iterator it2;
	for (it2 = descs.begin(); it2 != descs.end(); ++it2) {
		ss << (*it2).streamid << " " << (*it2).desc << "\n";
	}

	results["streams"] = ss.str();

	return 0;
}

int Server::dc_add_sink(KVS &params, KVS &results)
{
	return fc_add_sink(params, results);
}

int Server::dc_del_sink(KVS &params, KVS &results)
{
	return fc_del_sink(params, results);
}

int Server::dc_vs_exchange_position(KVS &params, KVS &results)
{
	char info[128];
	ost::MutexLock al(cs_conferences_);

	if (!chk_params(params, info, "cid", "id1", "id2", 0)) {
		results["reason"] = info;
		return -1;
	}

	int cid = atoi(params["cid"].c_str());
	Conference *conf = 0;
	CONFERENCES::iterator it;
	for (it = conferences_.begin(); it != conferences_.end(); ++it) {
		if ((*it)->cid() == cid) {
			conf = *it;
			break;
		}
	}

	if (!conf) {
		snprintf(info, sizeof(info), "cid=%d, NOT found!", cid);
		results["reason"] = info;
		return -1;
	}

	if (conf->vs_exchange_position(atoi(params["id1"].c_str()), atoi(params["id2"].c_str())) == 0) {
		return 0;
	}
	else {
		results["reason"] = "id error";
		return -1;
	}
}

int Server::dc_get_all_videos(KVS &params, KVS &results)
{
	char info[128];
	ost::MutexLock al(cs_conferences_);

	if (!chk_params(params, info, "cid", 0)) {
		results["reason"] = info;
		return -1;
	}

	int cid = atoi(params["cid"].c_str());
	Conference *conf = 0;
	CONFERENCES::iterator it;
	for (it = conferences_.begin(); it != conferences_.end(); ++it) {
		if ((*it)->cid() == cid) {
			conf = *it;
			break;
		}
	}

	if (!conf) {
		snprintf(info, sizeof(info), "cid=%d, NOT found!", cid);
		results["reason"] = info;
		return -1;
	}

	// 获取视频 stream 的描述，stream desc 部分使用 base64 编码，所以可以安全的使用 ; 进行分割
	// streams: <sid>,<desc>;<sid>,<desc>....
	{
		std::vector<StreamDescription> videos_desc = conf->get_stream_descriptions(100);
		std::stringstream ss;
		for (int i = 0; i < videos_desc.size(); i++) {
			ss << videos_desc[i].streamid << "," << videos_desc[i].desc << ";";
		}

		results["streams"] = ss.str();
	}

	{
		std::vector<SourceDescription> videos_desc = conf->get_source_descriptions(100);
		std::stringstream ss;
		for (int i = 0; i < videos_desc.size(); i++) {
			ss << videos_desc[i].sid << "," << videos_desc[i].desc << ";";
		}

		results["sources"] = ss.str();
	}

	return 0;
}

int Server::reg_notify(const char *from_jid, void *uac_token, KVS &params, KVS &results)
{
	char info[128];
	ost::MutexLock al(cs_conferences_);

	if (!chk_params(params, info, "cid", 0)) {
		results["reason"] = info;
		return -1;
	}

	int cid = atoi(params["cid"].c_str());
	Conference *conf = 0;
	CONFERENCES::iterator it;
	for (it = conferences_.begin(); it != conferences_.end(); ++it) {
		if ((*it)->cid() == cid) {
			conf = *it;
			break;
		}
	}

	if (!conf) {
		snprintf(info, sizeof(info), "cid=%d, NOT found!", cid);
		results["reason"] = info;
		return -1;
	}

	conf->reg_notify(from_jid, uac_token);
	return 0;
}

int Server::get_params(KVS &params, KVS &results)
{
	char info[128];
	ost::MutexLock al(cs_conferences_);

	if (!chk_params(params, info, "cid", 0)) {
		results["reason"] = info;
		return -1;
	}

	int cid = atoi(params["cid"].c_str());
	Conference *conf = 0;
	CONFERENCES::iterator it;
	for (it = conferences_.begin(); it != conferences_.end(); ++it) {
		if ((*it)->cid() == cid) {
			conf = *it;
			break;
		}
	}

	if (!conf) {
		snprintf(info, sizeof(info), "cid=%d, NOT found!", cid);
		results["reason"] = info;
		return -1;
	}

	return conf->getParams(params, results);
}

int Server::set_params(KVS &params, KVS &results)
{
	char info[128];
	ost::MutexLock al(cs_conferences_);

	if (!chk_params(params, info, "cid", 0)) {
		results["reason"] = info;
		return -1;
	}

	int cid = atoi(params["cid"].c_str());
	Conference *conf = 0;
	CONFERENCES::iterator it;
	for (it = conferences_.begin(); it != conferences_.end(); ++it) {
		if ((*it)->cid() == cid) {
			conf = *it;
			break;
		}
	}

	if (!conf) {
		snprintf(info, sizeof(info), "cid=%d, NOT found!", cid);
		results["reason"] = info;
		return -1;
	}

	return conf->setParams(params, results);
}

/** 获取 cpu, 网络，内存占用百分比 
		通过环境变量获取

		对于windows，似乎需要从注册表 HKEY_CURRENT_USER\\Environment 中读才行
 */
int Server::get_sys_info(double *cpu, double *mem, double *net_recv, double *net_send)
{
	*cpu = -1.0;
	*mem = -1.0;
	*net_recv = *net_send = -1.0;

#ifdef WIN32
	HKEY env = 0;
	LONG rc = RegOpenKeyA(HKEY_CURRENT_USER, "Environment", &env);
	if (rc == ERROR_SUCCESS) {
		DWORD type = REG_SZ;
		char buf[64];
		DWORD bufsize = sizeof(buf);

		rc = RegQueryValueExA(env, "zonekey_mcu_cpu_rate", 0, &type, (unsigned char*)buf, &bufsize);
		if (rc == ERROR_SUCCESS) {
			buf[bufsize] = 0;
			*cpu = atof(buf);
		}
		bufsize = sizeof(buf);
		rc = RegQueryValueExA(env, "zonekey_mcu_mem_rate", 0, &type, (unsigned char*)buf, &bufsize);
		if (rc == ERROR_SUCCESS) {
			buf[bufsize] = 0;
			*mem = atof(buf);
		}
		bufsize = sizeof(buf);
		rc = RegQueryValueExA(env, "zonekey_mcu_recv_rate", 0, &type, (unsigned char*)buf, &bufsize);
		if (rc == ERROR_SUCCESS) {
			buf[bufsize] = 0;
			*net_recv = atof(buf);
		}
		bufsize = sizeof(buf);
		rc = RegQueryValueExA(env, "zonekey_mcu_send_rate", 0, &type, (unsigned char*)buf, &bufsize);
		if (rc == ERROR_SUCCESS) {
			buf[bufsize] = 0;
			*net_send = atof(buf);
		}

		RegCloseKey(env);
	}
#else
	char *p = getenv("zonekey_mcu_cpu_rate");
	if (p) *cpu = atof(p);

	p = getenv("zonekey_mcu_mem_rate");
	if (p) *mem = atof(p);

	p = getenv("zonekey_mcu_recv_rate");
	if (p) *net_recv = atof(p);

	p = getenv("zonekey_mcu_send_rate");
	if (p) *net_send = atof(p);
#endif // 

	return 0;
}

static int zkrbt_mse_respond_log(void *token, const char *result, const char *param)
{
	log("\tresponse: result=%s, param=%s\n", result, param);
	return zkrbt_mse_respond(token, result, param);
}

void Server::callback_command(zkrobot_mse_t *mse_ins, const char *remote_jid, const char *str_cmd, const char *str_option, 
	                int is_delay, void *userdata, void *token)
{
	Server *This = (Server*)userdata;
	KVS params = util_parse_options(str_option);
	KVS results;

	log("[XMPP]: cmd=%s: cmd_opt=%s, from=%s\n", str_cmd, str_option, remote_jid);

	if (!strcmp("get_version", str_cmd) || !strcmp("version", str_cmd)) {
		if (token) zkrbt_mse_respond_log(token, "ok", VERSION_STR);
	}
	else if (!strcmp("sys_info", str_cmd)) {
		double cpu, mem, net1, net2;
		This->get_sys_info(&cpu, &mem, &net1, &net2);
		char info[128];
		snprintf(info, sizeof(info), "cpu=%.2f&mem=%.2f&net_recv=%.2f&net_send=%.2f", cpu, mem, net1, net2);
		if (token) zkrbt_mse_respond_log(token, "ok", info);
	}
	else if (!strcmp("create_conference", str_cmd)) {
		// 创建会议
		int rc = This->create_conference(params, results);
		if (token) zkrbt_mse_respond_log(token, rc < 0 ? "err" : "ok", util_encode_options(results).c_str());
	}
	else if (!strcmp("destroy_conference", str_cmd)) {
		// 删除会议
		int rc = This->destroy_conference(params, results);
		if (token) zkrbt_mse_respond_log(token, rc < 0 ? "err" : "ok", util_encode_options(results).c_str());
	}
	else if (!strcmp("list_conferences", str_cmd)) {
		// 列出所有会议
		int rc = This->list_conferences(params, results);
		if (token) zkrbt_mse_respond_log(token, rc < 0 ? "err" : "ok", util_encode_options(results).c_str());
	}
	else if (!strcmp("info_conference", str_cmd)) {
		// 查询会议详细信息
		int rc = This->info_conference(params, results);
		if (token) zkrbt_mse_respond_log(token, rc < 0 ? "err" : "ok", util_encode_options(results).c_str());
	}
	else if (!strcmp("reg_notify", str_cmd)) {
		if (This->reg_notify(remote_jid, token, params, results) < 0)
			if (token) zkrbt_mse_respond_log(token, "err", util_encode_options(results).c_str());
	}
	else if (!strcmp("set_params", str_cmd)) {
		int rc = This->set_params(params, results);
		if (token) zkrbt_mse_respond_log(token, rc < 0 ? "err" : "ok", util_encode_options(results).c_str());
	}
	else if (!strcmp("get_params", str_cmd)) {
		int rc = This->get_params(params, results);
		if (token) zkrbt_mse_respond_log(token, rc < 0 ? "err" : "ok", util_encode_options(results).c_str());
	}
	else if (!strcmp("fc.add_source", str_cmd)) {
		int rc = This->fc_add_source(params, results);
		if (token) zkrbt_mse_respond_log(token, rc < 0 ? "err" : "ok", util_encode_options(results).c_str());
	}
	else if (!strcmp("fc.del_source", str_cmd)) {
		int rc = This->fc_del_source(params, results);
		if (token) zkrbt_mse_respond_log(token, rc < 0 ? "err" : "ok", util_encode_options(results).c_str());
	}
	else if (!strcmp("fc.list_sources", str_cmd)) {
		int rc = This->fc_list_sources(params, results);
		if (token) zkrbt_mse_respond_log(token, rc < 0 ? "err" : "ok", util_encode_options(results).c_str());
	}
	else if (!strcmp("fc.add_sink", str_cmd)) {
		int rc = This->fc_add_sink(params, results);
		if (token) zkrbt_mse_respond_log(token, rc < 0 ? "err" : "ok", util_encode_options(results).c_str());
	}
	else if (!strcmp("fc.del_sink", str_cmd)) {
		int rc = This->fc_del_sink(params, results);
		if (token) zkrbt_mse_respond(token, rc < 0 ? "err" : "ok", util_encode_options(results).c_str());
	}
	else if (!strcmp("dc.add_stream", str_cmd)) {
		int rc = This->dc_add_stream(params, results, false);
		if (token) zkrbt_mse_respond_log(token, rc < 0 ? "err" : "ok", util_encode_options(results).c_str());
	}
	else if (!strcmp("dc.del_stream", str_cmd)) {
		int rc = This->dc_del_stream(params, results);
		if (token) zkrbt_mse_respond_log(token, rc < 0 ? "err" : "ok", util_encode_options(results).c_str());
	}
	else if (!strcmp("dc.add_stream_with_publisher", str_cmd)) {
		int rc = This->dc_add_stream(params, results, true);
		if (token) zkrbt_mse_respond_log(token, rc < 0 ? "err" : "ok", util_encode_options(results).c_str());
	}
	else if (!strcmp("dc.list_streams", str_cmd)) {
		int rc = This->dc_list_streams(params, results);
		if (token) zkrbt_mse_respond_log(token, rc < 0 ? "err" : "ok", util_encode_options(results).c_str());
	}
	else if (!strcmp("dc.add_source", str_cmd)) {
		int rc = This->dc_add_source(params, results);
		if (token) zkrbt_mse_respond_log(token, rc < 0 ? "err" : "ok", util_encode_options(results).c_str());
	}
	else if (!strcmp("dc.del_source", str_cmd)) {
		int rc = This->dc_del_source(params, results);
		if (token) zkrbt_mse_respond_log(token, rc < 0 ? "err" : "ok", util_encode_options(results).c_str());
	}
	else if (!strcmp("dc.add_sink", str_cmd)) {
		int rc = This->dc_add_sink(params, results);
		if (token) zkrbt_mse_respond_log(token, rc < 0 ? "err" : "ok", util_encode_options(results).c_str());
	}
	else if (!strcmp("dc.del_sink", str_cmd)) {
		int rc = This->dc_del_sink(params, results);
		if (token) zkrbt_mse_respond_log(token, rc < 0 ? "err" : "ok", util_encode_options(results).c_str());
	}
	else if (!strcmp("dc.get_all_videos", str_cmd)) {
		int rc = This->dc_get_all_videos(params, results);
		if (token) zkrbt_mse_respond_log(token, rc < 0 ? "err" : "ok", util_encode_options(results).c_str());
	}
	else if (!strcmp("dc.vs_exchange_position", str_cmd)) {
		int rc = This->dc_vs_exchange_position(params, results);
		if (token) zkrbt_mse_respond_log(token, rc < 0 ? "err" : "ok", util_encode_options(results).c_str());
	}
	else {
		if (token) zkrbt_mse_respond(token, "err", "UNKWON cmd");
	}
}
