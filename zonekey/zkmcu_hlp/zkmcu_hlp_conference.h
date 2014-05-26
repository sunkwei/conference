#ifndef _zkmcu_hlp_conference__hh
#define _zkmcu_hlp_conference__hh
#include "../xmpp/zkxmpp_ua.h"

enum CONFERECE_MODE
{
	CONFERENCE_MODE_FREE = 1, 
	CONFERENCE_MODE_DIRECT
};

typedef struct zkmcu_conf_ctx zkmcu_conf_ctx;

zkmcu_conf_ctx *zkmcu_hlp_conf_ctx_new(xmpp_ua_t *ua);
void zkmcu_hlp_conf_ctx_delete(zkmcu_conf_ctx *ctx);

typedef struct zkmcu_cf_ins zkmcu_cf_ins;
typedef void (*on_create_conf_result)(zkmcu_conf_ctx *ctx, zkmcu_cf_ins *cf_ins, int code, void *userdata);
int zkmcu_hlp_create_conference(zkmcu_conf_ctx *ctx, const char*mcu, CONFERECE_MODE mode, const char *description,
	                                                 on_create_conf_result cb, void *userdata);
int zkmcu_hlp_destroy_conference(zkmcu_conf_ctx *ctx, zkmcu_cf_ins *cf);

typedef struct zkmcu_media_endpoint zkmcu_media_endpoint;
const char *zkmcu_get_media_mcu(zkmcu_media_endpoint *endpoint);
unsigned int zkmcu_get_media_cid(zkmcu_media_endpoint *endpoint);
unsigned int zkmcu_get_media_sid(zkmcu_media_endpoint *endpoint);
const char *zkmcu_get_media_server_ip(zkmcu_media_endpoint *endpoint);
unsigned int zkmcu_get_media_rtp_port(zkmcu_media_endpoint *endpoint);
unsigned int zkmcu_get_media_rtcp_port(zkmcu_media_endpoint *endpoint);

//需要是请复制lsi, 回调结束后其指向内容被释放;
typedef void (*on_add_endpoint_result)(zkmcu_conf_ctx *ctx, 
	zkmcu_media_endpoint *lsi, int code, void *user_data);
int zkmcu_add_source(zkmcu_conf_ctx *ctx, zkmcu_cf_ins *cf, int payload, const char *description,
	on_add_endpoint_result cb, void *user_data);
int zkmcu_remove_source(zkmcu_conf_ctx *ctx,  zkmcu_media_endpoint *source);

int zkmcu_add_stream(zkmcu_conf_ctx *ctx, zkmcu_cf_ins *cf, int payload,
	on_add_endpoint_result cb, void *user_data);
int zkmcu_remove_stream(zkmcu_conf_ctx *ctx, zkmcu_media_endpoint *stream);

int zkmcu_add_sink(zkmcu_conf_ctx *ctx, const char *mcu, int cid, int sid, on_add_endpoint_result cb, void *user_data);
int zkmcu_remove_sink(zkmcu_conf_ctx *ctx, zkmcu_media_endpoint *sink);


#endif
