#ifndef _zksip__hh
#define _zksip__hh

#include "../common/zkconfig/zkconfig.h"

#ifdef __cplusplus
extern "C" {
#endif 

typedef struct zksip_callback
{
	/* 当收到对方ring消息后通知应用进程
	 * @param call_id zksip_make_call()设置的call id
	 * @param user_data 和sip实例绑定的用户数据
	 */
	void (*on_remote_ringning)(int call_id, void *user_data);
	
	/* 当收到对方接受消息后通知应用进程
	 * @param call_id zksip_make_call()设置的call id
	 * @param user_data 和sip实例绑定的用户数据
	 */
	void (*on_accepted)(int call_id, void *user_data);
    
	/* 当呼叫方呼叫失败时通知应用进程
	 * 修改时可给on_disconneted()增加参数code
	 * 将on_call_failed()并入on_disconnected()
	 * @param call_id zksip_make_call()设置的call id
	 * @param code 导致失败的错误码，例如被呼叫方拒绝时，code==603
	 * @param user_data 和sip实例绑定的用户数据
	 */
	void (*on_caller_failed)(int call_id, int code, void *user_data);
	
	/* 当收到呼叫后通知应用进程,该函数必须立即返回
	 * @param reomte_uri 呼叫方的uri
	 * @param call_id 为本次呼叫分配的call id
	 */
	void (*on_incoming_call)(int call_id, void *user_data);
	
	/* 当连接断开后通知应用进程
	 * @param call_id 本次通话的call id
	 * @param user_data 和sip实例绑定的用户数据
	 */
	void (*on_disconneted)(int call_id, void *user_data);	
}zksip_callback;

typedef struct zksip_cred_info
{
	char    *realm;	
    char	*username;	
    char	*pwd;		
}zksip_cred_info;

typedef struct zksip_register_cfg
{
	/* 可选的(可设为NULL），用于显示本地用户的用户名
	 */
	char *display_name;

	/** 
     * The full SIP URL for the account. The value can take name address or 
     * URL format, and will look something like "sip:account@serviceprovider".
     *
     * This field is mandatory.
	 * 这个字段是必须有的
     */
	char *id;
	
	/** 
     * This is the URL to be put in the request URI for the registration,
     * and will look something like "sip:serviceprovider".
     *
     * This field should be specified if registration is desired. If the
     * value is empty, no account registration will be performed.
	 * 如果需要注册，这个字段就是必须的
     */
	char *reg_uri;

	zksip_cred_info cred_info;

	/* 注册/注销成功或失败时调用
	 * @param status  注册/注销成功时status为0，否则为其他的错误码
	 * @param regc_data 用户注册时绑定的数据
	 */
	void (*zksip_regc_cb)(int status, void *regc_data);
}zksip_register_cfg;

typedef struct zksip_cfg
{
	zksip_callback cb;
	int max_calls; //可支持的call的最大数量（应大于0小于等于32）
	void *userdata; //用户需保证程序运行期间userdata一直有效

	zkconfig_t *conf;
}zksip_cfg;



/* 初始化 
 * @return 成功返回0，失败返回合适的错误码
 */
int zklib_init(void);


/* 创建sip实例
 * 注意：一个进程只能创建一个实例，即zksip_create只能调用一次
 * @param cfg 由用户填写的配置信息
 * @return 成功返回0, 失败返回合适的错误码
 */
int zksip_create(zksip_cfg *cfg);


/* run方法，处理zksip中的事件
 * @param msec_timeout 时间间隔，以毫秒计。不应该太大，推荐为10
 */
void zksip_run(int msec_timeout);

/* 注册，异步工作模式
 * @param regc_cfg 注册所需的配置信息
 * @return 当成功发送注册消息时，返回0；否则返回合适的错误码
 * 注意即使函数返回0，并不表示注册成功；应在函数返回0后，
 * 检查zksip_regc_cb（）中的status参数
 * 注册成功后，每隔一段时间自动重新注册，
 * 这时，每次都会调用regc_cfg中的zksip_regc_cb
 */
int zksip_register(zksip_register_cfg *regc_cfg, void *regc_data);

/* 取消注册
 * @return 成功发送取消注册的消息后返回0
 */
int zksip_unregister(void);

/* 呼叫指定的uri
 * @param dst_uri 要呼叫的用户的uri,例如 "sip:example@remote"
 * @param p_call_id 用来接收call_id的指针
 * @param sdp 本地sdp字符串
 * @return 如果成功返回0，否则返回引起失败的错误码
 */
int zksip_make_call(const char *dst_uri, int *p_call_id, const char *sdp);


/* 被叫方选择接受呼叫,一般在on_incoming_call()中调用
 * @param call_id 为本次通话创建的call id
 * @param sdp 本地sdp字符串
 */ 
void zksip_accept(int call_id, const char *sdp);

/* 被叫方选择拒绝呼叫,一般在on_incoming_call()中调用
 * @param call_id 为本次通话创建的call id
 */
void zksip_reject(int call_id, int code=603);


/* 挂断
 * 在呼叫方包括calling，ringing，confirmed的通话
 * 在被呼叫方包括confirmed通话
 * @param ins 由zksip_creat()返回的指针
 * @param call_id 要挂断的call的id
 * @return 成功返回0，否则返回引起失败的错误码,失败原因比如call_id不正确等
 */
int zksip_hang_up(int call_id);


/* 销毁创建的sip实例
 * 成功返回0，失败返回合适的错误码
 */
int zksip_destroy(void);

/* 得到远端的与call_id相关的sdp
 * @param call_id 
 * @return 如果有远端的sdp，返回远端sdp字符串，否则，返回NULL
 */
const char* zksip_get_remote_sdp(int call_id);

/* 得到远端的与call_id相关的uri
 * @param call_id 
 * @return 如果有远端的uri，返回远端uri字符串，否则，返回NULL
 */
const char* zksip_get_remote_uri(int call_id);



#ifdef __cplusplus
}
#endif 



#endif //_zksip__hh



