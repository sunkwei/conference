#include <stdio.h>
#include <stdlib.h>
#include <mediastreamer2/mediastream.h>
#include <mediastreamer2/msrtp.h>
#include <mediastreamer2/zk.h264.source.h>
#include "zkmcu_hlp_h264_source.h"
#include <string>
#include <map>
#include <cc++/thread.h>
#include <ortp/ortp.h>

namespace {
	// 构造 ZonekeyH264Source --> rtp sender 将h264视频数据发送到 server_ip+server_rtp_port
	class CTX : private ost::Thread
	{
		std::string server_ip_;
		int rtp_port_, rtcp_port_;
		ZonekeyH264SourceWriterParam writer_param_;
		ost::Event evt_, evt_quit_;
		bool quit_;
		rtcp_report_block_t rb_;

	public:
		CTX(const char *server, int rtp_port, int rtcp_port)
		{
			memset(&rb_, 0, sizeof(rb_));

			server_ip_ = server;
			rtp_port_ = rtp_port;
			rtcp_port_ = rtcp_port;

			quit_ = false;
			start();
			evt_.wait();	// 等待工作现成正常启动
			evt_.reset();
		}

		~CTX()
		{
			quit_ = true;
			evt_quit_.signal();
			join();
		}

		int send(const void *data, int len, double stamp)
		{
			return writer_param_.write(writer_param_.ctx, data, len, stamp);
		}

		int get_RR_report_block(rtcp_report_block_t *rb)
		{
			// *rb = rb_;
			rb->ssrc = ntohl(rb_.ssrc);
			rb->fl_cnpl = ntohl(rb_.fl_cnpl);
			rb->delay_snc_last_sr = ntohl(rb_.delay_snc_last_sr);
			rb->ext_high_seq_num_rec = ntohl(rb_.ext_high_seq_num_rec);
			rb->interarrival_jitter = ntohl(rb_.interarrival_jitter);
			rb->lsr = ntohl(rb_.lsr);
			return 0;
		}

	private:
		void run()
		{
			MSFilter *f_source, *f_rtp;
			MSTicker *ticker;
			RtpSession *rtp;

			rtp = rtp_session_new(RTP_SESSION_SENDONLY);
			rtp_session_set_rtp_socket_send_buffer_size(rtp, 3*1024*1024);
			rtp_session_set_rtp_socket_recv_buffer_size(rtp, 1024*1024);
			rtp_session_set_remote_addr_and_port(rtp, server_ip_.c_str(), rtp_port_, rtcp_port_);
			rtp_session_set_payload_type(rtp, 100);

			/*
			JBParameters jb;
			jb.adaptive = 1;
			jb.max_packets = 3000;
			jb.max_size = -1;
			jb.min_size = jb.nom_size = 300;
			rtp_session_set_jitter_buffer_params(rtp_, &jb);
			rtp_session_enable_jitter_buffer(rtp_, 0);
			*/
			// 禁用jitter buffer
			rtp_session_enable_adaptive_jitter_compensation(rtp, 0);
			rtp_session_enable_jitter_buffer(rtp, 0);

			f_rtp = ms_filter_new(MS_RTP_SEND_ID);
			ms_filter_call_method(f_rtp, MS_RTP_SEND_SET_SESSION, rtp);

			f_source = ms_filter_new_from_name("ZonekeyH264Source");
			ms_filter_call_method(f_source, ZONEKEY_METHOD_H264_SOURCE_GET_WRITER_PARAM, &writer_param_);

			ms_filter_link(f_source, 0, f_rtp, 0);

			// enable rtcp
			OrtpEvQueue *evq;
			evq = ortp_ev_queue_new();
			rtp_session_register_event_queue(rtp, evq);

			ticker = ms_ticker_new();
			ms_ticker_attach(ticker, f_source);

			evt_.signal();

			while (!quit_) {
				if (evt_quit_.wait(100)) {
					evt_quit_.reset();
					continue;
				}

				// 查询 rtcp ...
				OrtpEvent *ev = ortp_ev_queue_get(evq);
				while (ev) {
					OrtpEventType type = ortp_event_get_type(ev);
					if (type == ORTP_EVENT_RTCP_PACKET_RECEIVED) {
						process_rtcp_recved(ortp_event_get_data(ev)->packet);
					}

					ortp_event_destroy(ev);
					ev = ortp_ev_queue_get(evq);
				}
			}

			ms_ticker_detach(ticker, f_source);
			ms_ticker_destroy(ticker);

			rtp_session_unregister_event_queue(rtp, evq);
			ortp_ev_queue_destroy(evq);

			rtp_session_destroy(rtp);
			ms_filter_destroy(f_source);
			ms_filter_destroy(f_rtp);
		}

		void process_rtcp_recved(mblk_t *m)
		{
			/* FIXME: 此处收到 rtcp 包，一般为 RR，并且仅有一个 report block
			*/
			if (rtcp_is_RR(m)) {
				const rtcp_common_header *header = rtcp_get_common_header(m);
				for (int i = 0; i < header->rc; i++) {
					// XXX：一般来说，rc 应该等于 1
					const report_block_t *rb = rtcp_RR_get_report_block(m, i);
					if (rb) {
						rb_ = *((rtcp_report_block_t*)rb);
					}
				}
			}
		}
	};
};

zkmcu_hlp_h264_source_t *zkmcu_hlp_h264_source_open(const char *server_ip, int server_rtp_port, int server_rtcp_port)
{
	return (zkmcu_hlp_h264_source_t*)new CTX(server_ip, server_rtp_port, server_rtcp_port);
}

void zkmcu_hlp_h264_source_close(zkmcu_hlp_h264_source_t *ins)
{
	delete (CTX*)ins;
}

int zkmcu_hlp_h264_source_send(zkmcu_hlp_h264_source_t *ins, const void *data, int len, double stamp)
{
	return ((CTX*)ins)->send(data, len, stamp);
}

int zkmcu_hlp_h264_source_get_report_block(zkmcu_hlp_h264_source_t *ins, rtcp_report_block_t *rb)
{
	return ((CTX*)ins)->get_RR_report_block(rb);
}
