#define OLD_VIDEORENDER 1
#define ZKPLAYER 1

#include <stdio.h>
#include <stdlib.h>
#include <mediastreamer2/zk.yuv_sink.h>
#include <mediastreamer2/mediastream.h>
#include <mediastreamer2/msrtp.h>
#include <mediastreamer2/zk.void.h>
#include <mediastreamer2/rfc3984.h>
#include <cc++/thread.h>
#include <zonekey/zkH264Player.h>
#include "zkmcu_hlp_h264_render.h"
#include <string>
#include <vector>
#include <IPHlpApi.h>
#include <assert.h>
extern "C" {
#	include <libavcodec/avcodec.h>
#	include <libswscale/swscale.h>
}

#if OLD_VIDEORENDER
#	include <zonekey/video-inf.h>
#	pragma comment(lib, "render-video.lib")
#endif

#if ZKPLAYER
#	pragma comment(lib, "zkH264Player.lib")
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
static bool get_all_netinfs(std::vector<NetInf> &nis)
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
	static void log(const char *fmt, ...)
	{
		return;
		va_list mark;
		char buf[1024];

		va_start(mark, fmt);
		vsnprintf(buf, sizeof(buf), fmt, mark);
		va_end(mark);

		FILE *fp = fopen("hlp_render.log", "at");
		if (fp) {
			fprintf(fp, buf);
			fclose(fp);
		}
	}

	class DecoderUsingIntelMediaSDK
	{
		zkDshow *shower_;
		MSFilter *void_render_;	// 
		Rfc3984Context *unpacker_;
		MSQueue nals_;
		uint8_t *bitstream_;
		int bitstream_size_, bitstream_buf_;
		HWND hwnd_;

	public:

		DecoderUsingIntelMediaSDK(HWND hwnd) : hwnd_(hwnd)
		{
			shower_ = InitDshowPlayer(hwnd);
			if (!shower_) {
				MessageBoxA(0, "初始化 intel 解码器失败！", "致命错误", MB_OK);
				::exit(-1);
			}

			ZKPlay(shower_);

			void_render_ = ms_filter_new_from_name("ZonekeyVoidSink");
			if (!void_render_) {
				MessageBoxA(0, "初始化 void 接收器失败！", "致命错误", MB_OK);
				::exit(-1);
			}

			ZonekeyVoidSinkCallback cb;
			cb.opaque = this;
			cb.on_data = cb_data;
			ms_filter_call_method(void_render_, ZONEKEY_METHOD_VOID_SINK_SET_CALLBACK, &cb);

			unpacker_ = rfc3984_new();
			rfc3984_set_mode(unpacker_, 1);
			ms_queue_init(&nals_);

			bitstream_ = (uint8_t*)malloc(65536);
			bitstream_size_ = 0;
			bitstream_buf_ = 65536;
		}

		~DecoderUsingIntelMediaSDK()
		{
			log("%s: step 1\n", __FUNCTION__);
			ZKStop(shower_);
			log("%s: step 2\n", __FUNCTION__);
			UnInitDshowPlayer(shower_);
			log("%s: step 3\n", __FUNCTION__);
			rfc3984_destroy(unpacker_);
			log("%s: step 4\n", __FUNCTION__);
			free(bitstream_);
			log("%s: step 5\n", __FUNCTION__);
		}

		static bool support()
		{
			zkDshow *shower = InitDshowPlayer(0);
			if (shower)
				UnInitDshowPlayer(shower);

			return shower != 0;
		}

		MSFilter *sink_filter()
		{
			return void_render_;
		}

	private:
		static void cb_data(void *opaque, mblk_t *m)
		{
			((DecoderUsingIntelMediaSDK*)opaque)->cb_data(m);
		}

		void cb_data(mblk_t *m)
		{
			// TODO: 此处为 rtp，需要使用 rfc3984 解包，并且组成 annexb 串流格式，交给 decoder
			rfc3984_unpack(unpacker_, m, &nals_);
			if (!ms_queue_empty(&nals_)) {
				// 完整的 slices，
				uint8_t *bits;
				int need_init, key;
				int size = nalus2frame(&nals_, (void**)&bits, &need_init, &key);
				if (key) {
					log("%s: %p: slice: size=%d, need_init=%d, key=%d\n", __FUNCTION__, this, size, need_init, key);
					log("\t: %02x %02x %02x %02x %02x %02x %02x %02x\n",
						bits[0], bits[1], bits[2], bits[3], bits[4],
						bits[5], bits[6], bits[7]);
				}
				if (size > 0) {
					RECT rect;
					GetWindowRect(hwnd_, &rect);
					ResizeWindow(shower_, 0, 0, rect.right-rect.left, rect.bottom-rect.top);

					// 将 bits 写入 dec
					PutData(shower_, bits, size, 0);
				}
			}
		}

		// 从 nalus 组成 frame 字节流，annexb 格式
		int nalus2frame(MSQueue *nalus, void **bits, int *need_init, int *key)
		{
			mblk_t *m;
			uint8_t *dst = bitstream_;				// 缓冲
			uint8_t *end = dst + bitstream_buf_;	// 缓冲结束
			*need_init = 0;
			*key = 0;
			*bits = bitstream_;

			while (m = ms_queue_get(nalus)) {
				uint8_t *src = m->b_rptr;
				int nal_len = m->b_wptr - src;

				if (dst + nal_len + 32 > end) {
					// 需要扩展
					int pos = dst - bitstream_;
					int exp = (pos + nal_len + 32 + 4095) / 4096 * 4096;
					bitstream_ = (uint8_t*)realloc(bitstream_, exp);
					end = bitstream_ + exp;
					dst = pos + bitstream_;

					bitstream_buf_ = exp;
					
					*bits = bitstream_;
				}

				if (src[0] == 0 && src[1] && src[2] == 0 && src[3] == 1) {
					// 呵呵，不应该出现这个情况，说明发送时，没有从 annexb 转换
					memcpy(dst, src, nal_len);
					dst += nal_len;
				}
				else {
					// 将 nal 数据复制到 dst，添加 00 00 00 01
					uint8_t type = (*src) & ((1 << 5) - 1);
					if (type == 7) {
						// TODO: sps，检查是否变化，如果变化，则设置 need_init
						*key = 1;
					}
					if (type == 8) {
						// TODO: pps，检查是否变化，如果变化，则设置 need_init
						*key = 1;
					}
					if (type == 5) {
						// IDR frame
						*key = 1;
					}

					// 总是增加 00 00 00 01
					*dst++ = 0;
					*dst++ = 0;
					*dst++ = 0;
					*dst++ = 1;

					*dst++ = *src++;	// 复制 nal_type

					while (src < m->b_wptr - 3) {
						if (src[0] == 0 && src[1] == 0 && src[2] < 3) {
							// 需要特殊处理的码率字段
							*dst++ = 0;
							*dst++ = 0;
							*dst++ = 3;
							src += 2;
						}
						*dst++ = *src++;
					}
					// 循环中少了3个字节
					*dst++ = *src++;
					*dst++ = *src++;
					*dst++ = *src++;
				}

				freemsg(m);
			}

			return dst - bitstream_;
		}
	};

	class CTX : public ost::Thread
	{
		HWND hwnd_;
		std::string server_ip_;
		int server_rtp_port_, server_rtcp_port_;
		bool quit_;
#if ZKPLAYER
		DecoderUsingIntelMediaSDK *dec_;
#endif
#if OLD_VIDEORENDER
		VideoCtx show_;
#endif
		SwsContext *sws_;
		AVPicture pic_;
		int last_width_, last_height_;
		ost::Event evt_;
		bool rtp_recved_, rtcp_recved_;	// 用于不再发送打洞包
			
	public:
		on_rtcp_notify rtcp_notify_;
		ost::Mutex cs_callback_;
		void *rtcp_data_;
		rtp_stats_t stat_;

		CTX(void *hwnd, const char *server, int rtp, int rtcp)
		{
			rtp_recved_ = rtcp_recved_ = false;

			hwnd_ = (HWND)hwnd;
			server_ip_ = server;
			server_rtp_port_ = rtp;
			server_rtcp_port_ = rtcp;
			show_ = 0;
			last_width_ = 16;
			last_height_ = 16;

			sws_ = sws_getContext(last_width_, last_height_, PIX_FMT_YUV420P, last_width_, last_height_, PIX_FMT_RGB32, SWS_FAST_BILINEAR, 0, 0, 0);
			avpicture_alloc(&pic_, PIX_FMT_RGB32, last_width_, last_height_);

			quit_ = false;
			start();
			evt_.wait();	// 等待工作线程就绪
			evt_.reset();
		}

		~CTX()
		{
			quit_ = true;
			join();

			log("%s: en, thread end!\n", __FUNCTION__);

			avpicture_free(&pic_);
			log("%s: en pic freed!\n", __FUNCTION__);
			sws_freeContext(sws_);
			log("%s: en sws freed!\n", __FUNCTION__);
		}

		void run()
		{
#if ZKPLAYER
			if (DecoderUsingIntelMediaSDK::support()) {
				dec_ = new DecoderUsingIntelMediaSDK(hwnd_);

				log("%s: en, support intel media sdk\n", __FUNCTION__);
			}
			else {
				dec_ = 0;

				log("%s: :( unsupport intel media sdk!!!\n", __FUNCTION__);
			}
#endif

			MSFilter *f_rtp, *f_decoder = 0, *f_yuv = 0;
			MSTicker *ticker;
			RtpSession *rtp_session;
			
			static ost::Mutex cs_;

			cs_.enter();

			rtcp_notify_ = 0;
			memset(&stat_, 0, sizeof(stat_));

			// step 1: prepare rtp session

			rtp_session = rtp_session_new(RTP_SESSION_RECVONLY);
			rtp_session_set_rtp_socket_send_buffer_size(rtp_session, 1024*1024);
			rtp_session_set_rtp_socket_recv_buffer_size(rtp_session, 3*1024*1024);
			rtp_session_set_remote_addr_and_port(rtp_session, server_ip_.c_str(), server_rtp_port_, server_rtcp_port_);
			rtp_session_set_local_addr(rtp_session, util_get_myip(), 0, 0);
			rtp_session_set_payload_type(rtp_session, 100);

			// jitter buffer
			JBParameters jb;
			jb.adaptive = 1;
			jb.max_packets = 1000;
			jb.max_size = -1;
			jb.min_size = jb.nom_size = 300;	// 300ms
			rtp_session_set_jitter_buffer_params(rtp_session, &jb);
			/*
			// 禁用jitter buffer
			rtp_session_enable_adaptive_jitter_compensation(rtp_session, 0);
			rtp_session_enable_jitter_buffer(rtp_session, 0);
			*/
			// step 2: build graphs
			f_rtp = ms_filter_new(MS_RTP_RECV_ID);
			ms_filter_call_method(f_rtp, MS_RTP_RECV_SET_SESSION, rtp_session);

			show_ = 0;

#if ZKPLAYER
			if (dec_) {
				ms_filter_link(f_rtp, 0, dec_->sink_filter(), 0);
			}
			else {
#endif 

				f_decoder = ms_filter_new(MS_H264_DEC_ID);

				ZonekeyYUVSinkCallbackParam cbs;
				cbs.ctx = this;
				cbs.push = cb_yuv;
				f_yuv = ms_filter_new_from_name("ZonekeyYUVSink");
				ms_filter_call_method(f_yuv, ZONEKEY_METHOD_YUV_SINK_SET_CALLBACK_PARAM, &cbs);

				ms_filter_link(f_rtp, 0, f_decoder, 0);
				ms_filter_link(f_decoder, 0, f_yuv, 0);
#if ZKPLAYER
			}
#endif

			// step 3: run the graph
			ticker = ms_ticker_new();
			ms_ticker_attach(ticker, f_rtp);	// run the graph

			cs_.leave();

			evt_.signal();

			// enable rtcp
			OrtpEvQueue *evq;
			evq = ortp_ev_queue_new();
			rtp_session_register_event_queue(rtp_session, evq);

			// 激活发送端
			while (!quit_) {
				sleep(100);
				if (evq) {
					OrtpEvent *ev = ortp_ev_queue_get(evq);
					while (ev) {
						OrtpEventType type = ortp_event_get_type(ev);
						if (type == ORTP_EVENT_RTCP_PACKET_RECEIVED) {
							rtcp_recved_ = true;

							cs_callback_.enter();
							if (rtcp_notify_)
								rtcp_notify_((zkmcu_hlp_h264_render_t *)this, ev, rtcp_data_);
							cs_callback_.leave();
						}

						ortp_event_destroy(ev);

						ev = ortp_ev_queue_get(evq);
					}
				}

				// save last rtp stat
				stat_ = *rtp_session_get_stats(rtp_session);
				
				// 收到 rtp 和 rtcp 包之后，不再发送
				if (!rtp_recved_) active_sock(rtp_session_get_rtp_socket(rtp_session), server_ip_.c_str(), server_rtp_port_);
				if (!rtcp_recved_) active_sock(rtp_session_get_rtcp_socket(rtp_session), server_ip_.c_str(), server_rtcp_port_);
			}

			log("%s: en ... closing ...\n", __FUNCTION__);

			// step 5: release all
			ms_ticker_detach(ticker, f_rtp);
			ms_ticker_destroy(ticker);

			ms_filter_destroy(f_rtp);
#if ZKPLAYER
			if (dec_) {
				log("%s: %p: to delete dec_\n", __FUNCTION__, this);
				delete dec_;
				log("\t  ok\n");
				dec_ = 0;
			}
#endif
			if (f_yuv)
				ms_filter_destroy(f_yuv);
			if (f_decoder)
				ms_filter_destroy(f_decoder);


			rtp_session_unregister_event_queue(rtp_session, evq);
			ortp_ev_queue_destroy(evq);

			rtp_session_destroy(rtp_session);
			rtp_session = 0;

			log("%s: %p rtp session destroyed!\n", __FUNCTION__, this);

			if (show_) {
#if OLD_VIDEORENDER
				rv_close(show_);
#endif
				show_ = 0;
			}

			log("%s: %p working thread terminated!\n", __FUNCTION__, this);
		}

		void active_sock(int fd, const char *ip, int port)
		{
			sockaddr_in sin;
			sin.sin_family = AF_INET;
			sin.sin_port = htons(port);
			sin.sin_addr.s_addr = inet_addr(ip);

			::sendto(fd, "a", 1, 0, (sockaddr*)&sin, sizeof(sin));
		}

		static void cb_yuv(void *ctx, int width, int height, unsigned char *data[4], int stride[4])
		{
			((CTX*)ctx)->on_yuv(width, height, data, stride);
		}

		void on_yuv(int width, int height, unsigned char *data[4], int stride[4])
		{
			rtp_recved_ = true;

			// 此时收到完整图像，
			if (!show_) {
#if OLD_VIDEORENDER
				rv_open(&show_, hwnd_, width, height);
#endif
				PostMessage(hwnd_, 0x7789, 1, 0);
			}

			draw_yuv(width, height, data, stride);
			PostMessage(hwnd_, 0x7789, 1, 0);	// 呵呵
		}

		void draw_yuv(int width, int height, unsigned char *data[4], int stride[4])
		{
#if OLD_VIDEORENDER
			if (show_) {
				rv_writepic(show_, PIX_FMT_YUV420P, data, stride);
			}
#endif // 
		}
	};
};

zkmcu_hlp_h264_render_t *zkmcu_hlp_h264_render_open(void *hwnd, const char *server, int rtp_port, int rtcp_port)
{
	return (zkmcu_hlp_h264_render_t*)new CTX(hwnd, server, rtp_port, rtcp_port);
}

void zkmcu_hlp_h264_render_close(zkmcu_hlp_h264_render_t *ins)
{
	delete (CTX*)ins;
}

void zkmcu_hlp_h264_render_set_notify(zkmcu_hlp_h264_render_t *ins, on_rtcp_notify cb, void *userdata)
{
	CTX *ctx = (CTX *) ins;
	ost::MutexLock al(ctx->cs_callback_);
	ctx->rtcp_data_ = userdata;
	ctx->rtcp_notify_ = cb;
}

void zkmcu_hlp_h264_render_get_rtp_stat(zkmcu_hlp_h264_render_t *ins, rtp_stats_t *stat)
{
	CTX *ctx = (CTX*)ins;
	*stat = ctx->stat_;
}
