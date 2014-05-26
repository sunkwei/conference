#include <stdio.h>
#include <stdlib.h>
#include <mediastreamer2/mediastream.h>
#include <mediastreamer2/msrtp.h>
#include <mediastreamer2/zk.h264.source.h>
#include <mediastreamer2/zk.yuv_sink.h>
#include <zonekey/video-inf.h>
#include <string>
#include <cc++/thread.h>
#include <vector>
#include <assert.h>
#include "zk_win_video_render.h"
#ifdef WIN32
#	include <IPHlpApi.h>
#endif

#ifndef WIN32
#ifdef	HAVE_IOCTL_H
#include <ioctl.h>
#else
#include <sys/ioctl.h>
#endif
# ifdef HAVE_SYS_SOCKIO_H
# include <sys/sockio.h>
# endif
#ifdef	HAVE_NET_IF_H
#include <net/if.h>
#endif
#endif

// 判断 mac 是否为虚拟机 mac
static bool is_vm_mac(const char *mac)
{
		/** 
		"00:05:69"; //vmware1
		"00:0C:29"; //vmware2
		"00:50:56"; //vmware3
		"00:1C:42"; //parallels1
		"00:03:FF"; //microsoft virtual pc
		"00:0F:4B"; //virtual iron 4
		"00:16:3E"; //red hat xen , oracle vm , xen source, novell xen
		"08:00:27"; //virtualbox
		*/
	static const char *_vm_macs[] = {
		"000569",
		"000c29",
		"005056",
		"001c42",
		"0003ff",
		"000f4b",
		"00163e",
		"080027",
		0,
	};

	if (!mac) return true;	// FIXME:

	const char *p = _vm_macs[0];
	int n = 0;
	while (p) {
		if (strstr(mac, p))
			return true;
		n++;
		p = _vm_macs[n];
	}

	return false;
}

// 将 byte[] 类型，转换为小写的 ascii 字符串
static std::string conv_mac(unsigned char mac[], int len)
{
	std::string s;
	char *buf = (char *)alloca(len*2+1);
	buf[0] = 0;
	for (int pos = 0; len > 0; len--) {
		pos += sprintf(buf+pos, "%02x", *mac);
		mac++;
	}
	s = buf;
	return s;
}

// 描述一个网卡配置
struct NetInf
{
	std::string macaddr;
	std::vector<std::string> ips;
};

/** 获取可用网卡配置，仅仅选择启动的 ipv4的，非虚拟机的，ethernet 
 */
bool get_all_netinfs(std::vector<NetInf> &nis)
{
	nis.clear();
#ifdef WIN32
	ULONG len = 16*1024;
	IP_ADAPTER_ADDRESSES *adapter = (IP_ADAPTER_ADDRESSES*)malloc(len);

	// 仅仅 ipv4
	DWORD rc = GetAdaptersAddresses(AF_INET, 0, 0, adapter, &len);
	if (rc == ERROR_BUFFER_OVERFLOW) {
		adapter = (IP_ADAPTER_ADDRESSES*)realloc(adapter, len);
		rc = GetAdaptersAddresses(AF_INET, 0, 0, adapter, &len);
	}

	if (rc == 0) {
		IP_ADAPTER_ADDRESSES *p = adapter;
		while (p) {
			if ((p->IfType == IF_TYPE_ETHERNET_CSMACD || p->IfType == IF_TYPE_IEEE80211) &&
				(p->OperStatus == IfOperStatusUp)) {
				// 仅仅考虑 ethernet 或者 wifi，并且活动的
				// 不包括虚拟机的 mac
				std::string mac = conv_mac(p->PhysicalAddress, p->PhysicalAddressLength);
				if (!is_vm_mac(mac.c_str())) {
					NetInf ni;
					ni.macaddr = mac;

					IP_ADAPTER_UNICAST_ADDRESS *ip = p->FirstUnicastAddress;
					while (ip) {
						assert(ip->Address.lpSockaddr->sa_family == AF_INET);
						sockaddr_in *addr = (sockaddr_in*)ip->Address.lpSockaddr;
						ni.ips.push_back(inet_ntoa(addr->sin_addr));

						ip = ip->Next;
					}

					nis.push_back(ni);
				}
			}
			p = p->Next;
		}

		free(adapter);
	}

#else
	char buffer[8192];
	struct ifconf ifc;
	struct ifreq ifr;

	int fd = socket(AF_INET, SOCK_DGRAM, 0);
	if(fd == -1)
		return false;

	ifc.ifc_len = sizeof(buffer);
	ifc.ifc_buf = buffer;

	if(ioctl(fd, SIOCGIFCONF, &ifc) == -1)
		return false;

	int count = ifc.ifc_len / sizeof(ifreq);

	struct ifreq *req = ifc.ifc_req;
	struct ifreq *end = req + count;
	for (; req != end; ++req) {
		strcpy(ifr.ifr_name, req->ifr_name);
		ioctl(fd, SIOCGIFFLAGS, &ifr);
		if (ifr.ifr_flags & IFF_LOOPBACK)
			continue;	// lo

		sockaddr_in sin;
		sin.sin_addr = ((sockaddr_in&)req->ifr_addr).sin_addr;

		ioctl(fd, SIOCGIFHWADDR, &ifr);
		unsigned char mac_addr[6];
		memcpy(mac_addr, ifr.ifr_hwaddr.sa_data, 6);

		NetInf ni;
		ni.ips.push_back(inet_ntoa(sin.sin_addr));
		ni.macaddr = conv_mac(mac_addr, 6);

		nis.push_back(ni);
	}
	close(fd);

#endif
	return true;
}

static const char *util_get_myip()
{
	static std::string _ip;
	if (_ip.empty()) {
		std::vector<NetInf> nis;
		get_all_netinfs(nis);
		if (nis.size() > 0) {
			if (nis[0].ips.size() > 0) {
				_ip = nis[0].ips[0];
			}
		}
	}

	if (_ip.empty())
		_ip = "000.000.000.000";
	return _ip.c_str();
}

namespace {
class Ctx : public ost::Thread
{
	std::string ip_;
	int rtp_port_, rtcp_port_;
	void *hwnd_;
	VideoCtx render_;
	ost::Mutex cs_;
	ost::Event evt_;
	bool quit_;
	bool first_rtp_, first_rtcp_;

public:
	Ctx(void *hwnd, const char *server_ip, int server_rtp_port, int server_rtcp_port)
	{
		ip_ = server_ip, rtp_port_ = server_rtp_port, rtcp_port_ = server_rtcp_port;
		hwnd_ = hwnd;
		first_rtp_ = first_rtcp_ = true;
		render_ = 0;

		quit_ = false;
		start();
	}

	~Ctx()
	{
		quit_ = true;
		evt_.signal();
		join();
	}

	int send_h264(const void *data, int len, double stamp)
	{
		return -1;
	}

private:
	void run()
	{
		MSTicker *ticker;
		MSFilter *filter_rtp, *filter_decoder, *filter_sink;
		RtpSession *rtp;
		OrtpEvQueue *evq;

		// init RtpSession
		rtp = rtp_session_new(RTP_SESSION_RECVONLY);
		rtp_session_set_payload_type(rtp, 100);
		rtp_session_set_remote_addr_and_port(rtp, ip_.c_str(), rtp_port_, rtcp_port_);
		rtp_session_set_local_addr(rtp, util_get_myip(), 0, 0);
		
		JBParameters jb;
		jb.adaptive = 1;
		jb.max_packets = 1000;
		jb.max_size = -1;
		jb.min_size = 100;
		jb.nom_size = 100;
		rtp_session_set_jitter_buffer_params(rtp, &jb);

		evq = ortp_ev_queue_new();
		rtp_session_register_event_queue(rtp, evq);

		// init filter_rtp --> filter_decoder --> filter_sink
		filter_rtp = ms_filter_new(MS_RTP_RECV_ID);
		ms_filter_call_method(filter_rtp, MS_RTP_RECV_SET_SESSION, rtp);

		filter_decoder = ms_filter_new(MS_H264_DEC_ID);

		filter_sink = ms_filter_new_from_name("ZonekeyYUVSink");
		ZonekeyYUVSinkCallbackParam cb;
		cb.ctx = this;
		cb.push = cb_yuv;
		ms_filter_call_method(filter_sink, ZONEKEY_METHOD_YUV_SINK_SET_CALLBACK_PARAM, &cb);

		// link filters
		ms_filter_link(filter_rtp, 0, filter_decoder, 0);
		ms_filter_link(filter_decoder, 0, filter_sink, 0);

		// ticker 
		ticker = ms_ticker_new();

		ms_ticker_attach(ticker, filter_rtp);

		while (!quit_) {
			if (evt_.wait(200))
				continue;

			if (first_rtp_)
				active_sock(rtp_session_get_rtp_socket(rtp), ip_.c_str(), rtp_port_);

			if (first_rtcp_)
				active_sock(rtp_session_get_rtcp_socket(rtp), ip_.c_str(), rtcp_port_);

			// 此时处理 rtcp ...，如果还没有收到 rtp/rtcp，则发送激活包
			// TODO:
			OrtpEvent *ev = ortp_ev_queue_get(evq);
			while (ev) {
				// TODO: ..
				OrtpEventType type = ortp_event_get_type(ev);
				if (type == ORTP_EVENT_RTCP_PACKET_RECEIVED) {
					first_rtcp_ = false;
				}

				ortp_event_destroy(ev);
				ev = ortp_ev_queue_get(evq);
			}

		}

		// release all
		ms_ticker_detach(ticker, filter_rtp);
		ms_ticker_destroy(ticker);

		ms_filter_destroy(filter_rtp);
		ms_filter_destroy(filter_decoder);
		ms_filter_destroy(filter_sink);

		rtp_session_unregister_event_queue(rtp, evq);
		rtp_session_destroy(rtp);

		ortp_ev_queue_destroy(evq);

		if (render_) {
			rv_close(render_);
			render_ = 0;
		}
	}

	void active_sock(int sock, const char *ip, int port)
	{
		sockaddr_in sin;
		sin.sin_family = AF_INET;
		sin.sin_port = htons(port);
		sin.sin_addr.s_addr = inet_addr(ip);

		::sendto(sock, "abcd", 4, 0, (sockaddr*)&sin, sizeof(sin));
	}

	static void cb_yuv(void *opaque, int width, int height, unsigned char *data[4], int stride[4])
	{
		Ctx *This = (Ctx*)opaque;

		This->first_rtp_ = false;

		if (!This->render_) {
			rv_open(&This->render_, This->hwnd_, width, height);
		}

		if (This->render_) {
			rv_writepic(This->render_, PIX_FMT_YUV420P, data, stride);
		}
	}
};
};

void zkvr_init()
{
	ortp_init();
	ms_init();

	rtp_profile_set_payload(&av_profile, 100, &payload_type_h264);
	zonekey_yuv_sink_register();
}

zk_vr_t *zkvr_open(void *hwnd, const char *server_ip, int server_rtp_port, int server_rtcp_port)
{
	if (!IsWindow((HWND)hwnd)) {
		return 0;
	}
	return (zk_vr_t*)new Ctx(hwnd, server_ip, server_rtp_port, server_rtcp_port);
}

void zkvr_close(zk_vr_t *ins)
{
	delete ((zk_vr_t*)ins);
}

int zkvr_upload_h264(zk_vr_t *ins, const char *data, int len, double stamp)
{
	return ((Ctx*)ins)->send_h264(data, len, stamp);
}
