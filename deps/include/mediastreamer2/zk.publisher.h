/** 类似 tee + rtp sender filters，但不需要停止整个图 :-)

               ---------------------
    h264       |                    |
	   ------->|  zonekey.publisher |
	speex      |                    |
			   ---------------------
			             ^
						 |               add_remote(RtpSession *)
						 --------------  del_remote(RtpSession *)


		典型的，添加的 RtpSession 用于向 client 发送数据，如果 client 在 NAT 之后，正常情况下，必须在 socket 收到 client 的数据后，才能知道 client 的 ip:port
		所以调用 add_remote 时，一般情况下，无法正确填写 remote_addr_and_port 的！！！！！

		在 publisher 的内部实现中，当 add_remote(RtpSession *) 后，内部会首先在 RtpSession 的 rtp socket和 rtcp socket 上 recvfrom()，此时 client 在接受的同时，也
		需要发送特定的包。如果 recvfrom() 收到数据，则此时的 addr 就是 client 的通讯地址了，再加到 RtpSession 中；

		改为：如果外部没有指定 remote_ip_and_port，则内部执行 recvfrom()，如果 RtpSession 已经设置 addr and port，则直接加入 sending list!!!!!

		使用步骤：

				zonekey_publisher_register();
				.....

				MSFilter *pubisher = ms_filter_new_from_name("ZonekeyPubisher");
				ms_fiter_link(source_filter, 0, publisher, 0);
				ms_ticker_attach(ticker, publisher);
				.....


				RtpSession *rs = rtp_session_new(RTP_SESSION_SENDONLY);
				rtp_session_set_payload_type(....
				rtp_session_set_local_addr(rs, myip, 0, 0);	// 使用随机 rtp/rtcp port，此时 socket 都已经创建
				int rtp_port = rs->rtp.loc_port, rtcp_port = rs->rtcp.loc_port; // 将 rtp_port 和 rtcp_port 给 client，client 需要给这个发送数据，用于打洞；
				ms_filter_call_method(publisher, ZONEKEY_METHOD_PUBISHER_ADD_REMOTE, rs);
				......

				在 publisher 的 add_remote() 内部实现：
					不直接将该 rs 加入发送列表，而是加入 pending 列表
						rtp_sock = rtp_session_get_rtp_socket(), rtcp_socket = rtp_session_get_rtcp_socket();
						
						在 process() 中，针对每个 pending 列表中 socket 调用 ::recvfrom()，如果 rtp, rtcp 都得到 remote addr 了，则调用
						rtp_session_set_remote_addr_port()，并且将 rs 从 pending 列表中移动到发送列表中。

				client 的 rtp session 需要设置为 RTP_SESSION_SENDRECV 模式，当收到 publisher 的 rtp port, rtcp port 后，调用 rtp_session_add_remote_addr_port() 
				加入发送列表，并且周期发送一些数据即可。

 */

#ifndef zonekey_publisher__hh
#define zonekey_publisher__hh

#include "mediastream.h"
#include "allfilters.h"

#ifdef __cplusplus
extern "C" {
#endif // c++

MS2_PUBLIC void zonekey_publisher_register();
MS2_PUBLIC void zonekey_publisher_set_log_handler(void (*func)(const char *, ...));

// 希望别冲突吧 :)
#define ZONEKEY_ID_PUBLISHER (MSFilterInterfaceBegin+13)

// 添加删除接收者，一个接收者就是一个 RtpSession，ice 协商之类的，需要再外部搞定，publisher 本身仅仅傻傻地调用 rtp_session_sendm_with_ts()
#define ZONEKEY_METHOD_PUBLISHER_ADD_REMOTE				MS_FILTER_METHOD(ZONEKEY_ID_PUBLISHER, 0, void*)
#define ZONEKEY_METHOD_PUBLISHER_DEL_REMOTE				MS_FILTER_METHOD(ZONEKEY_ID_PUBLISHER, 1, void*)

// 设置 payload type
#define ZONEKEY_METHOD_PUBLISHER_SET_PAYLOAD			MS_FILTER_METHOD(ZONEKEY_ID_PUBLISHER, 2, int)

#ifdef __cplusplus
}
#endif // c++

#endif 
