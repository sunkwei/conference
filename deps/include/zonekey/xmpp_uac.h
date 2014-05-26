
#ifndef _xmpp_uac__hh
#define _xmpp_uac__hh

typedef struct uac_token_t uac_token_t;

#define MAX_GROUP_CNT 8

/** 一个联系人信息;
 */
typedef struct uac_roster_item{
	uac_roster_item *next;
	char *jid;
	char *name;
	char *subscription;	// both, from, to, none 描述订阅关系;  
	char *group[MAX_GROUP_CNT];		// 所属“组”，可能同时属于多个组, 最多8个;
	int group_cnt;	
} uac_roster_item;

#ifdef __cplusplus
extern "C" {
#endif

void zk_xmpp_uac_init(void);
void zk_xmpp_uac_shutdown(void);
	
typedef struct cb_xmpp_uac{
	
	void (*cb_receive_presence_available)(uac_token_t *token, char *remote_jid, int is_available, void *userdata);
	//返回值为1表示同意, 为0表示不同意;
	int (*cb_receive_subscription_req)(uac_token_t *token, char *remote_jid, int is_suscription, void *userdata);
	//返回值为1表示确认, 为0表示否认;
	int (*cb_receive_subscription_rsp)(uac_token_t *token, char *remote_jid, int is_promised, void *userdata);
	//roster_list为链表,要用free()逐个结点释放;
	void (*cb_receive_roster)(uac_token_t *token, uac_roster_item *roster_list, int is_update, void *userdata);

	/** 
		@param is_connected: 1,连接建立; 0, 连接断开;
	 */
	void (*cb_connect_notify)(uac_token_t *const token, int is_connected, void *userdata);
} cb_xmpp_uac;

//ret非NULL 为成功发送登录消息;是否登录成功查看cb_connect_notify;
uac_token_t *zk_xmpp_uac_log_in(const char *jid, const char *passwd, cb_xmpp_uac *cbs, void *opaque);

void zk_xmpp_uac_log_out(uac_token_t *token);

typedef struct zk_xmpp_uac_msg_token{
	char *remote_jid;
	char *cmd;
	char *option;
	void *userdata;
}zk_xmpp_uac_msg_token;

typedef void (*uac_receive_rsp)(zk_xmpp_uac_msg_token *rsp_token, const char *result, const char *option);
bool zk_xmpp_uac_send_cmd(uac_token_t *token, const char *remote_jid, const char *cmd, const char *option, void *opaque,
		            uac_receive_rsp cb);
	
enum uac_subscribe_type{
	UAC_OPT_SUBSCRIBE,
	UAC_OPT_UNSUBSCRIBE
};

int zk_xmpp_uac_subscribe(uac_token_t *token, const char *remote_jid, uac_subscribe_type opt);

typedef void (*cb_query_presence)(uac_token_t* token, const char *remote_jid, int is_present);
void zk_xmpp_uac_ping(uac_token_t *token, const char *remote_jid, cb_query_presence callback);

#ifdef __cplusplus
}
#endif

#endif

