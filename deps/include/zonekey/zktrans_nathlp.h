/** 封装 pjnath, 希望简化使用流程

		使用步骤:
			0. 实现回调函数
			1. 初始化, zktrans_nat_init() 返回 zktrans_nat_t 实例;
			2. 当需要发起通讯时, zktrans_nat_start(): 收集候选者;
			3. 调用 zktrans_nat_get_sdp(): 获取 sdp 字符串
			4. 通过某种途径, 与 callee 交换 sdp
			5. 使用被叫 sdp 进行协商, zktrans_nat_negotiate();
			6. 使用 zktrans_nat_send_rtp() 发送数据
			....

 */

#ifndef _zktrans_nathlp__hh
#define _zktrans_nathlp__hh

/** 支持 zkconfig 配置属性 
  	
  	对于 zktrans_nat，以下环境变量被声明

	1. zktrans_stun_sock_sndbuf: stun 发送缓冲字节大小，缺省 128k
	2. zktrans_stun_sock_rcvbuf: 接受缓冲，缺省 128k
	3. zktrans_turn_sock_sndbuf: turn 发送缓冲大小，缺省 128k
	4. zktrans_turn_sock_rcvbuf: 接收缓冲，缺省 128k
 */
#include "../common/zkconfig/zkconfig.h"

#ifdef __cplusplus
extern "C" {
#endif 

typedef struct zktrans_nat_t zktrans_nat_t;
/** 收到 rtp 数据 
		@param ins: zktrans_nat_init() 返回
		@param data: rtp 数据, 整个rtp包
		@param data_len: rtp数据长度
		@param userptr: 用户指针
 */
typedef void (*PFN_zktrans_nat_cb_rtp)(zktrans_nat_t *ins,
		const void *data, int data_len, void *userptr);
/** 收到 rtcp 数据 */
typedef PFN_zktrans_nat_cb_rtp PFN_zktrans_nat_cb_rtcp;

/** 收到异步通知
		@param ins: zktrans_nat_init() 返回
		@param type: 
				1 对应 zktrans_nat_start(), 
				2 对应 zktrans_nat_negotiate()
				100 指示当前 session 状态，状态使用 code， info为"状态名字"
		@param code: 0 完成, 否则错误编码
		@param userptr: 用户指针
 */
typedef void (*PFN_zktrans_nat_cb_state)(zktrans_nat_t *ins,
										 int type, int state,
										 const char *info,
										 void *opaque);

/** 初始化参数 */
struct zktrans_nat_cfg
{
	/** STUN srv */
	const char *stun_srv;
	int stun_port;

	/** TURN srv */
	const char *turn_srv;
	int turn_port;
	const char *turn_username;
	const char *turn_passwd;

	/** callback */
	PFN_zktrans_nat_cb_rtp cb_rtp;
	PFN_zktrans_nat_cb_rtcp cb_rtcp;
	PFN_zktrans_nat_cb_state cb_state;
	void *userptr;

	/** media type，将用于生成 sdp

			1 -> audio
			2 -> video
			3 -> subtitle

	 */
	int media_type;

	/** media payload type，用于构造 sdp
	 */
	int media_payload_type;

	/** 
		使用 cfg 配置，如果有效，将覆盖上述 STUN/TURN 设置
		@warning: 如果无效，必须设置为 0

			当前实现版本支持：
					zktrans_stun_sock_sndbuf: 缺省 128k
					zktrans_stun_sock_rcvbuf:
					zktrans_trun_sock_sndbuf:
					zktrans_turn_sock_rcvbuf:

					zktrans_stun_server: 指定stun server，将覆盖 zktrans_nat_cfg::stun_srv 设置
					zktrans_stun_server_port: 指定 stun server port，将覆盖 zktrans_nat_cfg::stun_port
					zktrans_turn_server:
					zktrans_turn_server_port:
					zktrans_turn_user:
					zktrans_turn_passwd:

					zktrans_poll_thread_timeout: poll线程超时时间，缺省 100ms，对于大码率视频，建议设置更短时间，如 20ms
	 */
	zkconfig_t *cfg;
};
typedef struct zktrans_nat_cfg zktrans_nat_cfg;


/** 初始化 */
zktrans_nat_t *zktrans_nat_init (zktrans_nat_cfg *cfg);

/** 释放 */
void zktrans_nat_uninit (zktrans_nat_t *ins);

/** 启动
		@param ins: zktrans_nat_init() 的返回
		@caller: 1 主叫, 0 被叫
		@warning: 将立即返回, 应该根据 PFN_zktrans_nat_cb_state() 检查
 */
void zktrans_nat_start (zktrans_nat_t *ins, int caller);

/** 停止
 */
void zktrans_nat_stop (zktrans_nat_t *ins);

/** 获取 sdp 字符串, 需要在 zktrans_nat_start 成功之后 */
const char *zktrans_nat_get_sdp (zktrans_nat_t *ins);

/** 使用 sdp 字符串协商 

		@param sdp: 对方 sdp 字符串
		@return > 0 成功， < 0 说明 sdp 格式错误！
		@warning 将立即返回, 使用 PFN_zktrans_nat_cb_state() 检查
 */
int zktrans_nat_negotiate (zktrans_nat_t *ins, const char *sdp);

/** 发送 rtp 数据
 */
int zktrans_nat_send_rtp (zktrans_nat_t *ins, 
		const char *data, int len);

/** 发送 rtcp 数据
 */
int zktrans_nat_send_rtcp (zktrans_nat_t *ins,
		const char *data, int len);

#ifdef __cplusplus
}
#endif 

#endif /** zktrans_nathlp.h */
