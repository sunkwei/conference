#include <stdio.h>
#include <stdlib.h>
#include <mediastreamer2/zk.publisher.h>
#include <ortp/ortp.h>
#include <vector>
#include <algorithm>
#include <map>
#include <assert.h>
#include <string>
#include <cc++/thread.h>

static void (*_log)(const char *, ...) = 0;

namespace {
/** 刚刚加入，还没有收到 client 数据
 */
struct PendingRS
{
	RtpSession *rs;
	std::string client_ip;
	int client_rtp_port, client_rtcp_port;

	PendingRS(RtpSession *rs)
	{
		this->rs = rs;
		client_rtp_port = client_rtcp_port = 0;
	}
};

class CTX
{
	int payload_type_;
	std::vector<RtpSession*> rtp_sessions_;	// 在 process() 中的发送
	ost::Mutex cs_rtp_sessions_;

	std::vector<PendingRS*> pendings_;	// 还没有得到 client ip, port 的
	ost::Mutex cs_pendings_;

public:
	CTX()
	{
		payload_type_ = 0;
	}

	~CTX()
	{
		// 释放掉不再使用的 pending ...
		ost::MutexLock al(cs_pendings_);
		std::vector<PendingRS*>::iterator it;
		for (it = pendings_.begin(); it != pendings_.end(); ++it) {
			delete *it;
		}
	}

	int set_payload(int pt)
	{
		// TODO: 此处应该检查一下. 
		payload_type_ = pt;
		return 0;
	}

	// 添加一个 rtp recver 
	int add_remote(RtpSession *rs)
	{
		// FIXME: 这里是否需要检查多次 push_back 呢？

		// FIXME：这里需要处理非 ipv4 的情况
		sockaddr_in *addr = (sockaddr_in*)&rs->rtp.rem_addr;
		if (addr->sin_family == 0 && addr->sin_port == 0) {
			// 没有设置，加入 pending list
			ost::MutexLock al(cs_pendings_);
			pendings_.push_back(new PendingRS(rs));
		}
		else {
			// 已经调用 rtp_session_set_remote_addr_and_port()，后果自负，呵呵
			ost::MutexLock al(cs_rtp_sessions_);
			rtp_sessions_.push_back(rs);
		}
		return 0;
	}

	// 删除一个 rtp recver
	int del_remote(RtpSession *rs)
	{
		do {
			// 检查 sending 列表
			ost::MutexLock al(cs_rtp_sessions_);
			std::vector<RtpSession*>::iterator it = std::find(rtp_sessions_.begin(), rtp_sessions_.end(), rs);
			if (it != rtp_sessions_.end()) {
				rtp_sessions_.erase(it);
			}
		} while (0);

		do {
			// 检查 pending 列表
			ost::MutexLock al(cs_pendings_);
			std::vector<PendingRS*>::iterator it;
			for (it = pendings_.begin(); it != pendings_.end(); ++it) {
				if ((*it)->rs == rs) {
					PendingRS *ps = *it;
					delete ps;
					pendings_.erase(it);
					break;
				}
			}
		} while (0);

		return 0;
	}

	// 在 ms ticker 中调用，从 inputs[0] 中取出数据，依次调用到所有 RtpSession
	// 依次检查 pendings_ 中是否有获取需要的信息
	void process(MSFilter *f)
	{
		// 完全由 inputs[0] 驱动
		assert(f->inputs[0]);

		mblk_t *m = ms_queue_get(f->inputs[0]);
		while (m) {
			dispatch_all(m);
			freemsg(m);	// 不再使用
			m = ms_queue_get(f->inputs[0]);
		}

		chk_pendings();
	}

private:
	// 将 m 转发给所有 rtp sessions
	void dispatch_all(mblk_t *m)
	{
		ost::MutexLock al(cs_rtp_sessions_);

		std::vector<RtpSession*>::iterator it;
		for (it = rtp_sessions_.begin(); it != rtp_sessions_.end(); ++it) {
			uint32_t ts = mblk_get_timestamp_info(m);
			
			// 下面这段来自 msrtp.c
			mblk_t *header = rtp_session_create_packet(*it, 12, 0, 0);
			rtp_set_markbit(header, mblk_get_marker_info(m));
			header->b_cont = dupmsg(m);						// 需要复制一份
			rtp_session_sendm_with_ts(*it, header, ts);
		}
	}

	class TryRecv
	{
		std::vector<RtpSession *> *rs_;
		ost::Mutex *cs_rs_;
	public:
		TryRecv(ost::Mutex *cs, std::vector<RtpSession *> *rs) : rs_(rs), cs_rs_(cs)
		{
		}

		void operator()(PendingRS *ps)
		{
			char buf[4096];
			sockaddr_in from;
			socklen_t len = sizeof(from);

			// 此时在 process() 工作线程中执行！
			if (ps->client_rtp_port == 0) {
				int sock = rtp_session_get_rtp_socket(ps->rs);

				if (recvfrom(sock, buf, sizeof(buf), MSG_PEEK, (sockaddr*)&from, &len) > 0) {
					// 收到数据
					ps->client_rtp_port = ntohs(from.sin_port);
					ps->client_ip = inet_ntoa(from.sin_addr);
				}
			}

			if (ps->client_rtcp_port == 0) {
				int sock = rtp_session_get_rtcp_socket(ps->rs);

				if (recvfrom(sock, buf, sizeof(buf), MSG_PEEK, (sockaddr*)&from, &len) > 0) {
					// 收到数据
					ps->client_rtcp_port = ntohs(from.sin_port);
					ps->client_ip = inet_ntoa(from.sin_addr);
				}
			}

			if (op_chk_recved(ps)) {
				// 加入 sending list
				ost::MutexLock al(*cs_rs_);
				rs_->push_back(ps->rs);
				fprintf(stderr, "========================= [publisher] OK, remote client: %s, %d.%d\n", ps->client_ip.c_str(), ps->client_rtp_port, ps->client_rtcp_port);
				rtp_session_set_remote_addr_and_port(ps->rs, ps->client_ip.c_str(), ps->client_rtp_port, ps->client_rtcp_port);
			}
		}
	};

	static bool op_chk_recved(PendingRS *rs)
	{
		return rs->client_rtp_port > 0 && rs->client_rtcp_port > 0;
	}

	// 检查 pendings_ 列表
	void chk_pendings()
	{
		ost::MutexLock al(cs_pendings_);
		std::vector<PendingRS*>::iterator it = pendings_.begin(), it2;

		// 尝试接收，如果完成，则加入 rtp_sessions_ 列表
		TryRecv tr(&cs_rtp_sessions_, &rtp_sessions_);
		std::for_each(pendings_.begin(), pendings_.end(), tr);

		// 删除所有已经收到的 ....
		pendings_.erase(std::remove_if(pendings_.begin(), pendings_.end(), op_chk_recved), pendings_.end());
	}
};
};

static void _init(MSFilter *f)
{
	CTX *ctx = new CTX;
	f->data = ctx;
}

static void _uninit(MSFilter *f)
{
	CTX *ctx = (CTX*)f->data;
	delete ctx;
	f->data = 0;
}

static void _process(MSFilter *f)
{
	CTX *ctx = (CTX*)f->data;
	ctx->process(f);
}

static int _method_set_payload(MSFilter *f, void *args)
{
	CTX *ctx = (CTX*)f->data;
	return ctx->set_payload(*(int*)args);
}

static int _method_add_remote(MSFilter *f, void *args)
{
	CTX *ctx = (CTX*)f->data;
	fprintf(stderr, "============[publisher] add rs\n");
	return ctx->add_remote((RtpSession*)args);
}

static int _method_del_remote(MSFilter *f, void *args)
{
	CTX *ctx = (CTX*)f->data;
	return ctx->del_remote((RtpSession*)args);
}

static MSFilterMethod _methods[] = 
{
	{ ZONEKEY_METHOD_PUBLISHER_SET_PAYLOAD, _method_set_payload },
	{ ZONEKEY_METHOD_PUBLISHER_ADD_REMOTE, _method_add_remote },
	{ ZONEKEY_METHOD_PUBLISHER_DEL_REMOTE, _method_del_remote },

	{ 0, 0 },
};

static MSFilterDesc _desc =
{
	MS_FILTER_PLUGIN_ID,
	"ZonekeyPublisher",
	"Zonekey publisher filter",
	MS_FILTER_OTHER,
	0,
	1,	// inputs
	0,	// outputs
	_init,
	0,
	_process,
	0,
	_uninit,
	_methods,
};

void zonekey_publisher_register()
{
	ms_filter_register(&_desc);
}

void zonekey_publisher_set_log_handler(void (*func)(const char *, ...))
{
	_log = func;
}
