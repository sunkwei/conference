#include <stdio.h>
#include <stdlib.h>
#include <mediastreamer2/zk.video.mixer.h>
#include <mediastreamer2/msrtp.h>

int main(int argc, char **argv)
{
	// 测试添加两路视频，
	// A 视频：从 rtp recv: 40000 接收，显示为整个背景
	// B 视频，从 rtp recv: 40010 接收，现实在 1/4 左上角，透明度 50%
	// 将合成视频流发布到 172.16.1.103:30000，方便利用 test_video_player 测试

	/**
			A: rtp recv(40000) ---\    p0                   p0 /--> 172.16.1.103:30000
			                       ---> zonekey.video.mixer --
			B: rtp recv(40010) ---/    p1        |          p1 \--> 172.16.1.xxx:30000
			                                     | px
												 ------> 直播服务/fms

	 */

	ortp_init();
	ms_init();
	//ortp_set_log_level_mask(ORTP_ERROR);

	// only support h264
	rtp_profile_set_payload(&av_profile, 102, &payload_type_h264);

	// rtp sessions
	RtpSession *rs_recv_a, *rs_send_a, *rs_recv_b, *rs_send_b;
	rs_recv_a = rtp_session_new(RTP_SESSION_RECVONLY);						rs_recv_b = rtp_session_new(RTP_SESSION_RECVONLY);
	rtp_session_set_local_addr(rs_recv_a, "0.0.0.0", 40000, 40001);			rtp_session_set_local_addr(rs_recv_b, "0.0.0.0", 40010, 40011);
	rtp_session_set_profile(rs_recv_a, &av_profile);						rtp_session_set_profile(rs_recv_b, &av_profile);
	rtp_session_set_payload_type(rs_recv_a, 102);							rtp_session_set_payload_type(rs_recv_b, 102);
	rtp_session_set_jitter_compensation(rs_recv_a, 200);					rtp_session_set_jitter_compensation(rs_recv_b, 200);

	rs_send_a = rtp_session_new(RTP_SESSION_SENDONLY);						rs_send_b = rtp_session_new(RTP_SESSION_SENDONLY);
	rtp_session_set_remote_addr_and_port(rs_send_a, "127.0.0.1", 50000, 50001);  rtp_session_set_remote_addr_and_port(rs_send_b, "127.0.0.1", 30000, 30001);
	rtp_session_set_local_addr(rs_send_a, "0.0.0.0", -1, -1);				rtp_session_set_local_addr(rs_send_b, "0.0.0.0", -1, -1);	
	rtp_session_set_profile(rs_send_a, &av_profile);						rtp_session_set_profile(rs_send_b, &av_profile);
	rtp_session_set_payload_type(rs_send_a, 102);							rtp_session_set_payload_type(rs_send_b, 102);
	rtp_session_set_rtp_socket_send_buffer_size(rs_send_a, 1024*1024);		rtp_session_set_rtp_socket_send_buffer_size(rs_send_b, 1024*1024);

	JBParameters jitter;
	jitter.max_packets = 500;
	jitter.adaptive = 1;
	jitter.max_size = -1;
	jitter.min_size = jitter.nom_size = 200;	// ms

	rtp_session_set_jitter_buffer_params(rs_send_a, &jitter);

	// MSFilter for rtp
	MSFilter *f_rtp_recv_a = ms_filter_new(MS_RTP_RECV_ID), *f_rtp_recv_b = ms_filter_new(MS_RTP_RECV_ID);
	MSFilter *f_rtp_send_a = ms_filter_new(MS_RTP_SEND_ID), *f_rtp_send_b = ms_filter_new(MS_RTP_SEND_ID);

	ms_filter_call_method(f_rtp_recv_a, MS_RTP_RECV_SET_SESSION, rs_recv_a);		ms_filter_call_method(f_rtp_recv_b, MS_RTP_RECV_SET_SESSION, rs_recv_b);
	ms_filter_call_method(f_rtp_send_a, MS_RTP_SEND_SET_SESSION, rs_send_a);		ms_filter_call_method(f_rtp_send_b, MS_RTP_SEND_SET_SESSION, rs_send_b);

	// h264 decoder
	MSFilter *f_decoder_a = ms_filter_new(MS_H264_DEC_ID), *f_decoder_b = ms_filter_new(MS_H264_DEC_ID);

	// init zonekey.video.mixer filter
	zonekey_video_mixer_register();
	MSFilterDesc *desc = ms_filter_lookup_by_name("ZonekeyVideoMixer");
	MSFilter *mixer = ms_filter_new_from_desc(desc);

	// 申请两路 channel
	int id_a, id_b;
	// 必须设置到 mixer，两个 YUV 输出的位置
	ZonekeyVideoMixerChannelDesc ch_desc_a, ch_desc_b;
	ch_desc_a.x = 0;								ch_desc_b.x = 100;
	ch_desc_a.y = 0;								ch_desc_b.y = 20;
	ch_desc_a.width = 960;							ch_desc_b.width = 500;
	ch_desc_a.height = 540;							ch_desc_b.height = 290;
	ch_desc_a.alpha = 255;							ch_desc_b.alpha = 210;

	ms_filter_call_method(mixer, ZONEKEY_METHOD_VIDEO_MIXER_GET_CHANNEL, &id_a);
	ch_desc_a.id = id_a;
	ms_filter_call_method(mixer, ZONEKEY_METHOD_VIDEO_MIXER_SET_CHANNEL_DESC, &ch_desc_a);

	ms_filter_call_method(mixer, ZONEKEY_METHOD_VIDEO_MIXER_GET_CHANNEL, &id_b);
	ch_desc_b.id = id_b;
	ms_filter_call_method(mixer, ZONEKEY_METHOD_VIDEO_MIXER_SET_CHANNEL_DESC, &ch_desc_b);

	// link filters
	ms_filter_link(f_rtp_recv_a, 0, f_decoder_a, 0);							ms_filter_link(f_rtp_recv_b, 0, f_decoder_b, 0);
	ms_filter_link(f_decoder_a, 0, mixer, id_a);								ms_filter_link(f_decoder_b, 0, mixer, id_b);
	ms_filter_link(mixer, 1, f_rtp_send_a, 0);	 // mixer output[1] 用于输出 x264 流

	// MSTicker
	MSTicker *ticker = ms_ticker_new();
	ms_ticker_attach(ticker, mixer);

	// 跑吧
	while (1) {
		ms_sleep(1);
	}

	return 0;
}
