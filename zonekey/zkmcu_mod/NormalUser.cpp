#include "StdAfx.h"
#include "NormalUser.h"
#include <algorithm>
#include <mediastreamer2/mediastream.h>
#include <ortp/ortp.h>
#include <mediastreamer2/zk.void.h>
#include <mediastreamer2/zk.aac.h>
#include <mediastreamer2/zk.ilbc.h>
#include <mediastreamer2/zk.rtmp.livingcast.h>

NormalUser *NormalUser::_instance = 0;

static const char *get_domain()
{
	return getenv("xmpp_domain");
}

static const char *get_user_jid()
{
	static std::string _jid;

	if (_jid.empty()) {
		std::stringstream ss;
		ss << "normaluser" << "@" << get_domain();
		_jid = ss.str();
	}

	return _jid.c_str();
}

void NormalUser::cb_connect_notify(uac_token_t *const token, int is_connected, void *userdata)
{
	NormalUser *This = (NormalUser*)userdata;
	if (is_connected)
		This->connected_ = true;
	else {
		This->connected_ = false;
		cb_xmpp_uac cbs = { 0, 0, 0, 0, cb_connect_notify };	// FIXME: 短线总是重连 ...
		This->uac_ = zk_xmpp_uac_log_in(get_user_jid(), "ddkk1212", &cbs, This);
	}
}

unsigned NormalUser::_proc_run(void *p)
{
	((NormalUser*)p)->proc_run();
	return 0;
}

NormalUser::NormalUser(void)
{
	// FIXME: 这里顺便初始化 mediastreamer2 库
	ortp_init();
	ms_init();
	zonekey_void_register();
	zonekey_aac_codec_register();
	libmsilbc_init();
	ortp_set_log_level_mask(ORTP_ERROR);

	rtp_profile_set_payload(&av_profile, 100, &payload_type_h264);
	rtp_profile_set_payload(&av_profile, 102, &payload_type_ilbc);

	zk_xmpp_uac_init();

	connected_ = false;
	quit_ = false;
	th_ = _beginthreadex(0, 0, _proc_run, this, 0, 0);

	const char *domain = get_domain();
	
	cb_xmpp_uac cbs = { 0, 0, 0, 0, cb_connect_notify };
	uac_ = zk_xmpp_uac_log_in(get_user_jid(), "ddkk1212", &cbs, this);
}


NormalUser::~NormalUser(void)
{
	zk_xmpp_uac_log_out(uac_);
	zk_xmpp_uac_shutdown();
}

void NormalUser::proc_run()
{
	// 工作线程 ....
	while (!quit_) {
		if (!is_ok()) {
			ost::Thread::sleep(100);	// 
			continue;
		}

		if (evt_task_added_.wait(100)) {
			evt_task_added_.reset();
			if (is_ok()) {
				send_all_pending_tasks();
			}
		}
	}
}

static bool op_remove_sent(XmppTask *task)
{
	if (task->sent) {
		delete task;
		return true;
	}
	else
		return false;
}

void NormalUser::send_all_pending_tasks()
{
	ost::MutexLock al(cs_tasks_);
	TASKS::iterator it;

	for (it = tasks_.begin(); it != tasks_.end(); ++it) {
		if (!is_ok())
			break;

		XmppTask *task = *it;
		zk_xmpp_uac_send_cmd(uac_, task->remote_jid.c_str(), task->cmd.c_str(), task->option.c_str(), task->opaque, task->cb);
		task->sent = true;
	}

	tasks_.erase(std::remove_if(tasks_.begin(), tasks_.end(), op_remove_sent), tasks_.end());
}
