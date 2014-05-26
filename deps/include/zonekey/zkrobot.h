#ifndef _zkrobot__hh
#define _zkrobot__hh

/** 基于 XMPP，
	主要实现以下功能：
		1. 注册：
		2. 处理订阅事件：
		3. 处理命令：
		4. 发送命令，并获取相应

	应用场景主要分为两类，主动和被动，类似sip中的 uac和 uas 的概念

	主动：
		zkrbt_create(...);
		zkrbt_request(...);
		...

	被动：
		zkrbt_create(...);
		
		zkrbt_callback.command() {
			...
			str = zkrbt_alloc(size);
			return str;
		}
 */

typedef struct zkrobot_t zkrobot_t;

#ifdef __cplusplus
extern "C" {
#endif 

/** 全局通知消息 */
typedef struct zkrbt_callback
{
	/** 当收到订阅事件时的处理 

			@param opaque: create 提供的参数
			@param jid: 订阅者的 jid
			@param op: 1 订阅，2 取消订阅
			@return 1: 允许，2：拒绝
	 */
	int (*subscribed)(void *opaque, const char *jid, int op);

	/** 收到对方命令
		
			@param jid: 发送命令者
			@param cmd_str: 命令字符串，是0结束的字符串
			@return: 返回需要返回的字符串，必须使用 zkrbt_alloc() 分配内存，而且使用 0 结束，如果不希望回复，直接返回 null 即可
	 */
	char *(*command)(void *opaque, const char *jid, const char *cmd_str);

	/** 网络状态通知
		
			@param code: 描述网络事件
							0: 连接成功；
							-1：网络被中断
	 */
	void (*connect_notify)(void *opaque, int code);
} zkrbt_callback;

/** 一个联系人信息
 */
typedef struct zkrbt_roster_contact
{
	const char *jid;
	const char *name;
	const char *subscription;	// both, from, to, none 描述订阅关系
	const char **group;			// 所属“组”，可能同时属于多个组
	int group_cnt;

} zkrbt_roster_contact;


/** 初始化库，调用其他 zkrbt_xxx 之前，必须首先调用
 */
void zkrbt_init ();

/** 构造一个实例

		@param ins: 出参，如果成功，则返回对象实例
		@param jid: jabberid
		@param passwd: 注册到 jabber 服务器所需的口令
		@param cbs: 回调通知接口
		@param opaque:
		@return  0 成功；
				-1 
 */
int zkrbt_create (zkrobot_t **ins, const char *jid, const char *passwd, zkrbt_callback *cbs, void *opaque);

/** 释放实例 */
void zkrbt_close (zkrobot_t *ins);

/** 实现自注册，阻塞实现，返回0成功，否则错误值
 */
int zkrbt_register_ibr(const char *jid, const char *passwd, const char *email);

/** 发送一个 request，并期待收到 response，一般用于 client 的行为

		@param response: 如果非0，将在收到 res 后回调
 */
int zkrbt_request (zkrobot_t *ins, const char *jid, const char *str, void (*response)(void *opaque, int code, const char *str), void *opaque);

/** 分配内存
 */
char *zkrbt_alloc (int size);

/** 订阅某个 jid，将在 callback.subscribe 中通知订阅是否成功
 */
int zkrbt_subscribe (zkrobot_t *ins, const char *jid);

/** 取消订阅某个 jid，将在 callback.subscribe 中通知是否成功
 */
int zkrbt_unsubscribe (zkrobot_t *ins, const char *jid);

/** 返回花名册信息

		@param ins:
		@param contacts: 出参，返回所有花名册中的联系人信息列表，必须使用 zkrbt_free_contact() 释放每一条
		@param cnt: 出参，contacts 有效数目
		@return < 0 失败
 */
int zkrbt_get_roster (zkrobot_t *ins, zkrbt_roster_contact ***contacts, int *cnt);

/** 释放 contact 结构
 */
void zkrbt_free_contacts (zkrobot_t *ins, zkrbt_roster_contact **contacts, int cnt);

#ifdef __cplusplus
}
#endif

#endif /** zkrobot.h */
