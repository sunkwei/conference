/** 基于 xmpp 实现原来的mse模型中服务注册

		// 初始化设置
		ctx = zkrbt_mse_open("app.zonekey.com.cn");	// 指定xmpp server

		// 注册服务，callback_cmd 为命令回调
		zkr_mse_reg_service(ctx, service_name, service_type, service_url, callback_cmd, opaque);

		// 注册设备
		zkr_mse_reg_device(ctx, device_name, device_type, device_desc,callback_cmd, opaque);

		// 释放
		zkrbt_mse_close(ctx);

	内部实现：
		注册服务：
			zkrbt_mse_reg_service():
				if (zkrbt_create(...) failure)
					zkrbt_register_irb(....)
				if (zkrbt_create(...) failure)
					error

				zkrbt_get_roster()
				if (root@domain is not in contacts)
					zkrbt_subscribe(root@...)

				当 command 为 internal.describe 时，返回服务信息

 */

#ifndef _zkrobot_mse__hh
#define _zkrobot_mse__hh

#ifdef __cplusplus
extern "C" {
#endif /* c++ */

#define ROBOT_MSE_VERSION "0.0.1"

typedef struct zkrbt_mse_t zkrbt_mse_t;

/** 设备属性，如果无效，应为 null
 */
struct zkrbt_mse_device_type
{
	const char *vendor;
	const char *model;
	const char *version;
	const char *info;
};

/** 当收到 ReqCmd 时，回调，对于 service一般是请求服务，对于设备，一般是配置
		@param opaque
		@param jid: 发送消息的 jid
		@param cmd_str: 命令字符串
		@param cmd_options: 命令参数字符串
		@param result_str: 出参，执行结果，必须调用 zkrbt_mse_malloc() 分配内存，将由库负责释放! 可以为 null
		@return 0 成功，-1 失败
 */
typedef int (*PFN_zkrbt_mse_command)(void *opaque, const char *jid, const char *cmd_str, const char *cmd_options, char **result_str);

zkrbt_mse_t *zkrbt_mse_open(const char *xmpp_domain);
void zkrbt_mse_close(zkrbt_mse_t *ctx);

int zkrbt_mse_reg_service(zkrbt_mse_t *ctx, const char *service_name, const char *service_type, const char *service_url,
						  PFN_zkrbt_mse_command callback_cmd, void *opaque);
int zkrbt_mse_reg_device(zkrbt_mse_t *ctx, const char *device_name, const char *device_type, const struct zkrbt_mse_device_type *desc,
						 PFN_zkrbt_mse_command callback_cmd, void *opaque);

// 分配内存
void *zkrbt_mse_malloc(int size);


#ifdef __cplusplus
}
#endif /* c++ */

#endif /* zkrobot_mse.h */
