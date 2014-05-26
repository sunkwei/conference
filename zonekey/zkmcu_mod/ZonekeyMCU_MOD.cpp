// ZonekeyMCU_MOD.cpp : CZonekeyMCU_MOD 的实现

#include "stdafx.h"
#include "ZonekeyMCU_MOD.h"
#include "SinkH264.h"
#include "SinkiLBC.h"

/// 用于方便将异步事件转换为同步
class SyncParam
{
	HANDLE evt_compl_;
	const void *opaque_;
	std::string result_, result_options_;

public:
	SyncParam(const void *opaque)
	{
		evt_compl_ = CreateEvent(0, 0, 0, 0);
		opaque_ = opaque;
	}

	~SyncParam()
	{
		CloseHandle(evt_compl_);
	}

	bool wait_complete(int timeout)
	{
		return WaitForSingleObject(evt_compl_, timeout) == WAIT_OBJECT_0;
	}

	void signal()
	{
		SetEvent(evt_compl_);
	}

	const void *opaque() const 
	{
		return opaque_;
	}

	void save_res(const char *result, const char *result_options)
	{
		if (result)
			result_ = result;
		else
			result_ = "";

		if (result_options)
			result_options_ = result_options;
		else
			result_options_ = "";
	}

	const char *result() const { return result_.c_str(); }
	const char *result_options() const { return result_options_.c_str(); }
};

// CZonekeyMCU_MOD

STDMETHODIMP CZonekeyMCU_MOD::get_MCUJid(BSTR* pVal)
{
	*pVal = SysAllocString(mcu_jid_);
	return S_OK;
}


STDMETHODIMP CZonekeyMCU_MOD::put_MCUJid(BSTR newVal)
{
	mcu_jid_ = newVal;
	return S_OK;
}

STDMETHODIMP CZonekeyMCU_MOD::Start(LONG cid, LONG payload, LONG sid)
{
	cid_ = cid, payload_ = payload, sid_ = sid;

	// 启动工作线程
	quit_ = false;
	th_ = (HANDLE)_beginthreadex(0, 0, proc_run, this, 0, 0);

	return S_OK;
}


STDMETHODIMP CZonekeyMCU_MOD::Stop(void)
{
	if (th_ == 0)
		return S_OK;

	// 结束，首先检查是否在工作线程中调用的
	if (GetCurrentThread() == th_) {
		// 此时必定在回调中执行的，此时只要设置结束标记即可
		quit_ = true;
	}
	else {
		// 等待工作线程结束
		quit_ = true;
		WaitForSingleObject(th_, -1);
		th_ = 0;
	}

	return S_OK;
}

unsigned CZonekeyMCU_MOD::proc_run(void *p)
{
	CZonekeyMCU_MOD *This = (CZonekeyMCU_MOD*)p;
	return This->proc_run();
}

unsigned CZonekeyMCU_MOD::proc_run()
{
	// 申请一个 sink 
	std::string mcu_ip;
	int mcu_rtp_port, mcu_rtcp_port;
	int sink_id = create_sink(NormalUser::Instance(), this->cid_, this->sid_, this->payload_, mcu_ip, mcu_rtp_port, mcu_rtcp_port);
	if (sink_id == -1) {
		// 失败，直接通知 CBError
		// FIXME: 应该通知出尽量详细的信息
		_bstr_t info("无法创建到 mcu 的sink对象");
		CBError(EC_CREATE_SINK, info);

		if (rtmp_) {
			Close_Rtmp(rtmp_);
			UnInit_Rtmp(rtmp_);
			rtmp_ = 0;
		}

		if (flv_) {
			flv_writer_close(flv_);
			flv_ = 0;
		}

		return -1;
	}

	// 此时启动媒体接收 ...
	SinkBase *sink = 0;
	if (payload_ == 100) {
		sink = new SinkH264(mcu_ip.c_str(), mcu_rtp_port, mcu_rtcp_port, cb_data, this);
	}
	else if (payload_ == 102) {
		// ilbc audio
		sink = new SinkiLBC(mcu_ip.c_str(), mcu_rtp_port, mcu_rtcp_port, cb_data, this);
	}

	if (sink) sink->Start();

	// 等待结束 ...
	while (!quit_) {
		ost::Thread::sleep(100);
		if (sink)
			sink->RunOnce();
	}

	if (sink) {
		sink->Stop();
		delete sink;
	}

	// 释放 sink
	destroy_sink(NormalUser::Instance(), sink_id, this->cid_);

	if (rtmp_) {
		Close_Rtmp(rtmp_);
		UnInit_Rtmp(rtmp_);
		rtmp_ = 0;
	}

	if (flv_) {
		flv_writer_close(flv_);
		flv_ = 0;
	}

	return 0;
}

// 收到 mcu 的返回
static void cb_res(zk_xmpp_uac_msg_token *rsp_token, const char *result, const char *option)
{
	SyncParam *sp = (SyncParam*)rsp_token->userdata;
	sp->save_res(result, option);
	sp->signal();
}

int CZonekeyMCU_MOD::create_sink(NormalUser *user, int cid, int sid, int payload, std::string &ip, int &rtp, int &rtcp)
{
	/** 申请对 sid 的点播，阻塞知道收到 mcu 的返回，如果成功，则填充 ip, rtp, rtcp，并且返回 0，失败返回 -1 */
	rtp = rtcp = 0;

	char options[128];
	snprintf(options, sizeof(options), "cid=%d&sid=%d&payload=%d", cid, sid, payload);
	SyncParam *sp = new SyncParam(0);
	user->addTask(this->mcu_jid_, "dc.add_sink", options, sp, cb_res);
	if (!sp->wait_complete(30000)) {
		// 说明 30 秒内没有收到相应，可以认为已经失败了！！！！
		delete sp;
		return -1;
	}
	
	if (!strcmp(sp->result(), "ok")) {
		// mcu 同意，此时需要解析 result_options 的内容，必定为 sinkid=xxx&server_ip=xxx&server_rtp_port=xx&server_rtcp_port=xx
		// TODO:
		char *ro = strdup(sp->result_options());
		int sinkid = -1;
		char *p = strtok(ro, "&");
		while (p) {
			if (!strncmp(p, "sinkid=", 7)) {
				sinkid = atoi(p+7);
			}
			else if (!strncmp(p, "server_ip=", 10)) {
				ip = p+10;
			}
			else if (!strncmp(p, "server_rtp_port=", 16)) {
				rtp = atoi(p+16);
			}
			else if (!strncmp(p, "server_rtcp_port=", 17)) {
				rtcp = atoi(p+17);
			}

			p = strtok(0, "&");
		}

		free(ro);
		delete sp;
		return sinkid;
	}
	else {
		// mcu 返回失败！
		delete sp;
		return -1;
	}
}

void CZonekeyMCU_MOD::destroy_sink(NormalUser *user, int sink_id, int cid)
{
	char options[128];
	snprintf(options, sizeof(options), "cid=%d&sinkid=%d", cid, sink_id);
	SyncParam *sp = new SyncParam(0);
	user->addTask(this->mcu_jid_, "dc.del_sink", options, sp, cb_res);
	sp->wait_complete(30000);

	// FIXME: 不关心是否成功，失败了 :(
	delete sp;
}

void CZonekeyMCU_MOD::cb_data(double stamp, void *data, int len, bool key)
{
	/// 收到来自 sink 的回调数据，此数据需要打包，然后调用 CBMediaFrameGot()
	VARIANT vd;
	VariantInit(&vd);
	vd.vt = VT_ARRAY | VT_UI1;	// 无符号字节数组类型

	SAFEARRAYBOUND rgs;
	rgs.lLbound = 0;
	rgs.cElements = len;
	vd.parray = SafeArrayCreate(VT_UI1, 1, &rgs);
	if (vd.parray == 0) {
		// ???: 内存分配失败？？
		return;
	}
	void *ptr;
	HRESULT hr = SafeArrayAccessData(vd.parray, &ptr);
	if (hr != S_OK) {
		SafeArrayDestroy(vd.parray);
		return;
	}

	memcpy(ptr, data, len);
	SafeArrayUnaccessData(vd.parray);

	this->CBMediaFrameGot(this->payload_, stamp, vd, key ? -1 : 0);	// notify!

	SafeArrayDestroy(vd.parray);	// release all
}

STDMETHODIMP CZonekeyMCU_MOD::put_RecordFilename(BSTR newVal)
{
	// TODO: 在此添加实现代码
	bstr_t Filename(newVal);
	std::string filename = Filename;
	
	flv_ = flv_writer_open(filename.c_str());

	return S_OK;
}


STDMETHODIMP CZonekeyMCU_MOD::put_RtmpURL(BSTR newVal)
{
	// TODO: 在此添加实现代码
	bstr_t Url(newVal);
	std::string url = Url;

	Init_Rtmp(&rtmp_);
	if (Open_Rtmp(rtmp_, url.c_str()) == 1) {
		return S_OK;
	}
	else {
		UnInit_Rtmp(rtmp_);
		rtmp_ = 0;
	}

	return S_OK;
}
