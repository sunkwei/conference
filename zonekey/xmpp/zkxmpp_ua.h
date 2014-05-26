#ifndef _zkxmpp_ua__hh
#define _zkxmpp_ua__hh
#include <strophe.h>

__declspec(dllimport) xmpp_ctx_t *_xmpp_ctx;
typedef struct xmpp_ua_t xmpp_ua_t;

typedef int (*xmpp_ua_cb)(xmpp_ua_t *ua, xmpp_stanza_t *stanza, void *userdata);

void xmpp_ua_init(void);
void xmpp_ua_uninit(void);

xmpp_ua_t *xmpp_ua_new(void);
void xmpp_ua_delete(xmpp_ua_t *ua);

typedef void (*xmpp_ua_conn_handler)(xmpp_ua_t *ua, int code, void *userdata);
void xmpp_ua_conn_handler_add(xmpp_ua_t *ua, xmpp_ua_conn_handler cb, void *userdata);
void xmpp_ua_conn_handler_remove(xmpp_ua_t *ua, xmpp_ua_conn_handler cb);
void *xmpp_ua_conn_get_userdata(xmpp_ua_t *ua, xmpp_ua_conn_handler cb);

 typedef int (*xmpp_ua_handler)(xmpp_ua_t *ua, xmpp_stanza_t *stanza, void *userdata);

 int xmpp_ua_login(xmpp_ua_t *ua, const char *id, const char *pwd);
 void  xmpp_ua_logout(xmpp_ua_t *ua);
int xmpp_ua_present(xmpp_ua_t *ua);
 int  query_roster(xmpp_ua_t *ua, xmpp_ua_handler cb, void *userdata);

void xmpp_ua_msg_handler_add(xmpp_ua_t *ua, xmpp_ua_handler msg_cb, void *userdata);
void xmpp_ua_iq_handler_add(xmpp_ua_t *ua, xmpp_ua_handler iq_cb, void *userdata);
void xmpp_ua_presence_handler_add(xmpp_ua_t *ua, xmpp_ua_handler presence_cb, void *userdata);
void xmpp_ua_msg_handler_remove(xmpp_ua_t *ua, xmpp_ua_handler cb);
void xmpp_ua_presence_handler_remove(xmpp_ua_t *ua, xmpp_ua_handler cb);
void xmpp_ua_iq_handler_remove(xmpp_ua_t *ua, xmpp_ua_handler cb);
void *xmpp_ua_msg_get_userdata(xmpp_ua_t *ua, xmpp_ua_handler cb);
void *xmpp_ua_presence_get_userdata(xmpp_ua_t *ua, xmpp_ua_handler cb);
void *xmpp_ua_iq_get_userdata(xmpp_ua_t *ua, xmpp_ua_handler cb);
//id_handler处理完后自动移除, 目前不需要持久存在;
void xmpp_ua_id_handler_add(xmpp_ua_t *ua, xmpp_ua_cb cb, char *id, void *userdata);
int xmpp_ua_get_unique_id(void);
void xmpp_ua_get_unique_string(xmpp_ua_t *ua, char buf[128]);
int xmpp_ua_send(xmpp_ua_t *ua, xmpp_stanza_t *stanza);

//目前无法完全正确识别mse命令和普通msg;
//认为格式符合mse命令格式的即为mse命令;
typedef void (*xmpp_ua_mse_receive_rsp)(xmpp_ua_t *ua, const char *remote, const char *result, const char *option, void *userdata);
int xmpp_ua_mse_send_cmd(xmpp_ua_t *ua, const char *remote_jid, const char *cmd, const char *option, 
											xmpp_ua_mse_receive_rsp cb, void *userdata);
//转化字符串中的'&', '<'等关键字;
//result 要用free释放;
void xmpp_ua_change_string(const char *str_to_change, char **result);

const char *xmpp_ua_get_jid(xmpp_ua_t *ua);
#endif

