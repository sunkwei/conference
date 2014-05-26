#pragma once

#include <zonekey/xmpp_uac.h>

// 描述一个需要处理的任务，当 NormalUser 可用时（login 成功），发送到指定的 jid。并在收到 response 后，...
struct XmppTask
{
	std::string remote_jid;
	std::string cmd;
	std::string option;
	void *opaque;
	uac_receive_rsp cb;
	bool sent;
};

/** 一个单件类，对应着登录到 xmpp server 的 normaluser 用户，用于发送接收 xmpp message
	总是将命令缓存，当 is_ok() 是发送 ....
 */
class NormalUser
{
	NormalUser(void);
	~NormalUser(void);
	static NormalUser *_instance;
	bool connected_;
	uac_token_t *uac_;
	bool quit_;
	ost::Mutex cs_tasks_;
	typedef std::vector<XmppTask *> TASKS;
	TASKS tasks_, tasks_waiting_res_;	// 一个用于缓存发送的列表，一个用于等待收到 respond
	ost::Event evt_task_added_;
	uintptr_t th_;

public:
	static NormalUser *Instance()
	{
		if (!_instance)
			_instance = new NormalUser;
		return _instance;
	}

	void addTask(const char *remote_jid, const char *cmd, const char *option, void *opaque, uac_receive_rsp cb)
	{
		XmppTask *task = new XmppTask;
		task->remote_jid = remote_jid;
		task->cmd = cmd;
		task->option = option;
		task->opaque = opaque;
		task->cb = cb;

		ost::MutexLock al(cs_tasks_);
		tasks_.push_back(task);
		evt_task_added_.signal();
	}

private:
	static void cb_connect_notify(uac_token_t *const token, int is_connected, void *userdata);
	static unsigned __stdcall _proc_run(void *p);	// 工作线程
	void proc_run();
	bool is_ok() const { return connected_; }	// 返回是否可用，
	void send_all_pending_tasks();	// 发送所有 tasks
};
