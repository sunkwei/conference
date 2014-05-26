#ifndef _zkrobot_mse_ex__hh
#define _zkrobot_mse_ex__hh

#ifdef __cplusplus
extern "C" {
#endif

void zkrbt_mse_init(const char *domain);
void zkrbt_mse_shutdown(void);

enum zkrobot_mse_catalog
{
	ZKROBOT_SERVICE,
	ZKROBOT_DEVICE,
	ZKROBOT_LOGIC,
	ZKROBOT_OFFLINE
};

typedef struct zkrobot_mse{
	char id[256];
	//char resource[256];
	zkrobot_mse_catalog catalog;
	union {
		struct {
			char url[256];
		} service;

		struct  {
			char vendor[256];
			char model[256];
			char version[64];
			char info[256];
		} device;
	}mse_ins;
	
	char type[32];
} zkrobot_mse;

typedef struct zkrobot_mse_t zkrobot_mse_t;

typedef struct zk_mse_cbs{
/** 
	当收到message传来的命令(reqcmd)时调用;
	@param remote_jid: 命令发出者的jid;
	@param str_cmd: 命令字符串;
	@param str_option: 命令参数;
	@param is_delay: 该命令是否是离线命令. 0, 不是; 1, 是;
	@param userdata: zkrbt_login()传入的opaque指针;
	@param token: 用于zkrbt_respond, 当其为NULL时, 不需要用户回复;
	*/
void (*PFN_zkrbt_mse_command)(zkrobot_mse_t *mse_ins, const char *remote_jid, const char *str_cmd, const char *str_option, 
	                int is_delay, void *userdata, void *token);

/**
	是否登录成功;
*/
void (*PFN_zkrbt_mse_log_in)(zkrobot_mse_t * mse_ins, int is_successful, void *userdata);
}zk_mse_cbs;

/** 
	成功创建mse返回实例指针, 失败返回NULL;
	是否登录成功要看PFN_zkrbt_mse_log_in回调;
*/
zkrobot_mse_t *zkrbt_mse_new(zkrobot_mse *mse, zk_mse_cbs *cbs, void *userdata);
void zkrbt_mse_destroy(zkrobot_mse_t *rbt_mse);

int zkrbt_mse_respond(void *token, const char *result, const char *param);

#ifdef __cplusplus
}
#endif

#endif


