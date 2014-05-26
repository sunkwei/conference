#include "zkmcu_hlp_conference.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>


struct zkmcu_conf_ctx
{
	xmpp_ua_t *ua;
};

zkmcu_conf_ctx *zkmcu_hlp_conf_ctx_new(xmpp_ua_t *ua)
{
	zkmcu_conf_ctx *ctx = (zkmcu_conf_ctx *) malloc(sizeof(zkmcu_conf_ctx));
	ctx->ua = ua;
	return ctx;
}
void zkmcu_hlp_conf_ctx_delete(zkmcu_conf_ctx *ctx)
{
	free(ctx);
}

struct mse_option_table
{
	char *key;
	char *value;
};

static void _get_mse_option_table(mse_option_table table[], int count, const char *option)
{
	char *option_cpy = strdup(option);
	char *param_value = strtok(option_cpy, "&");
	while (param_value)
	{
		char *equal_sign = strchr(param_value, '=');
		if (equal_sign)
		{
			*equal_sign = '\0';
			char *value = strdup(equal_sign + 1);

			for (int i = 0; i < count; i++)
			{
				if (!strcmp(param_value, table[i].key))
				{
					table[i].value = value;
				}
			}
		}
		param_value = strtok(0, "&");
	}
	free(option_cpy);
}

struct zkmcu_cf_ins
{
	char *mcu;
	int cid;
};
struct create_cf_data
{
	zkmcu_conf_ctx *ctx;
	char *mcu;
	on_create_conf_result cb;
	void *userdata;
};

static void zkmcu_create_cf_rsp(xmpp_ua_t *ua, const char *remote, const char *result, const char *option, void *userdata)
{
	create_cf_data *data = (create_cf_data *)userdata;

	if (!strcmp(result, "ok"))
	{	
		int cid;
		sscanf(option, "cid=%d", &cid);
		zkmcu_cf_ins *cf = (zkmcu_cf_ins *) malloc(sizeof(zkmcu_cf_ins));
		cf->cid = cid;
		cf->mcu = strdup(data->mcu);
		data->cb(data->ctx, cf, 0, data->userdata);
	}
	else
		data->cb(data->ctx, NULL,  -1, data->userdata);
	free(data->mcu);
	free(data);
}
int zkmcu_hlp_create_conference(zkmcu_conf_ctx *ctx, const char*mcu, CONFERECE_MODE mode, const char *description,
	on_create_conf_result cb, void *userdata)
{
	char *cf_mode = "director";
	if (mode == CONFERENCE_MODE_FREE)
	{
		cf_mode = "free";
	}
	char *cmd_options = (char *)malloc(32+strlen(description));
	sprintf(cmd_options, "mode=%s&desc=%s", cf_mode, description);
	create_cf_data *data = (create_cf_data *) malloc(sizeof(create_cf_data));
	data->cb = cb;
	data->ctx = ctx;
	data->userdata = userdata;
	data->mcu = strdup(mcu);
	return xmpp_ua_mse_send_cmd(ctx->ua, mcu, "create_conference", cmd_options, zkmcu_create_cf_rsp, data);

}

int zkmcu_hlp_destroy_conference(zkmcu_conf_ctx *ctx, const char *mcu, int cid)
{
	char option[32];
	sprintf(option, "cid=%d", cid);
	return xmpp_ua_mse_send_cmd(ctx->ua, mcu, "destroy_conference", option, NULL, NULL);
}


struct zkmcu_media_endpoint
{
	zkmcu_cf_ins cf;
	int sid;
	char *server_ip;
	unsigned int rtp_port;
	unsigned int rtcp_port;
};

const char *zkmcu_get_media_mcu(zkmcu_media_endpoint *endpoint)
{
	return endpoint->cf.mcu;
}
unsigned int zkmcu_get_media_cid(zkmcu_media_endpoint *endpoint)
{
	return endpoint->cf.cid;
}
unsigned int zkmcu_get_media_sid(zkmcu_media_endpoint *endpoint)
{
	return endpoint->sid;
}
const char *zkmcu_get_media_server_ip(zkmcu_media_endpoint *endpoint)
{
	return endpoint->server_ip;
}
unsigned int zkmcu_get_media_rtp_port(zkmcu_media_endpoint *endpoint)
{
	return endpoint->rtp_port;
}
unsigned int zkmcu_get_media_rtcp_port(zkmcu_media_endpoint *endpoint)
{
	return endpoint->rtcp_port;
}

struct add_endpoint_data
{
	zkmcu_conf_ctx *ctx;
	zkmcu_cf_ins cf;
	on_add_endpoint_result cb;
	void *userdata;
};

static void _add_source_result(xmpp_ua_t *ua, const char *remote, const char *result, const char *option, void *userdata)
{
	add_endpoint_data *data = (add_endpoint_data *) userdata;
	if (!strcmp(result, "ok"))
	{
		zkmcu_media_endpoint *lsi  = (zkmcu_media_endpoint *)malloc(sizeof(zkmcu_media_endpoint));
		lsi->cf = data->cf;
		mse_option_table params[4];
		params[0].key = strdup("sid");
		params[1].key = strdup("server_ip");
		params[2].key = strdup("server_rtp_port");
		params[3].key = strdup("server_rtcp_port");
		_get_mse_option_table(params, 4, option);

		lsi->sid = atoi(params[0].value);
		lsi->server_ip = params[1].value;
		lsi->rtp_port = atoi(params[2].value);
		lsi->rtcp_port = atoi(params[3].value);
		data->cb(data->ctx, lsi, 0, data->userdata);
		for (int i = 0; i < 4; i++)
		{
			free(params[i].key);
			free(params[i].value);
		}
		free(lsi);
	}
	else
	{
		data->cb(data->ctx, NULL, -1, data->userdata);
	}
	
	free(data);
}

int zkmcu_add_source(zkmcu_conf_ctx *ctx, zkmcu_cf_ins *cf, int payload, const char *description,
	on_add_endpoint_result cb, void *user_data)
{
	add_endpoint_data *data = (add_endpoint_data *) malloc(sizeof(add_endpoint_data));
	data->ctx = ctx;
	data->cf = *cf;
	data->cb = cb;
	data->userdata = user_data;
	char option[64];
	sprintf(option, "cid=%d&payload=%d", cf->cid, payload);
	return xmpp_ua_mse_send_cmd(ctx->ua, cf->mcu, "add_source", option,  _add_source_result, data);
}

int zkmcu_remove_source(zkmcu_conf_ctx *ctx,  zkmcu_media_endpoint *source)
{
	char option[64];
	sprintf(option, "cid=%d&sid=%s", source->cf.cid, source->sid);
	return xmpp_ua_mse_send_cmd(ctx->ua, source->cf.mcu, "del_source", option, NULL, NULL);
}

static void _add_stream_result(xmpp_ua_t *ua, const char *remote, const char *result, const char *option, void *userdata)
{
	add_endpoint_data *data = (add_endpoint_data *) userdata;
	if (!strcmp(result, "ok"))
	{
		zkmcu_media_endpoint *lsi  = (zkmcu_media_endpoint *)malloc(sizeof(zkmcu_media_endpoint));
		lsi->cf = data->cf;
		mse_option_table params[4];
		params[0].key = strdup("streamid");
		params[1].key = strdup("server_ip");
		params[2].key = strdup("server_rtp_port");
		params[3].key = strdup("server_rtcp_port");
		_get_mse_option_table(params, 4, option);

		lsi->sid = atoi(params[0].value);
		lsi->server_ip = params[1].value;
		lsi->rtp_port = atoi(params[2].value);
		lsi->rtcp_port = atoi(params[3].value);
		data->cb(data->ctx, lsi, 0, data->userdata);
		for (int i = 0; i < 4; i++)
		{
			free(params[i].key);
			free(params[i].value);
		}
		free(lsi);
	}
	else
	{
		data->cb(data->ctx, NULL, -1, data->userdata);
	}

	free(data);
}
int zkmcu_add_stream(zkmcu_conf_ctx *ctx, zkmcu_cf_ins *cf, int payload,
	on_add_endpoint_result cb, void *user_data)
{
	add_endpoint_data *data = (add_endpoint_data *) malloc(sizeof(add_endpoint_data));
	data->ctx = ctx;
	data->cf = *cf;
	data->cb = cb;
	data->userdata = user_data;
	char option[64];
	sprintf(option, "cid=%d&payload=%d", cf->cid, payload);
	return xmpp_ua_mse_send_cmd(ctx->ua, cf->mcu, "dc.add_stream", option,  _add_stream_result, data);
}

int zkmcu_remove_stream(zkmcu_conf_ctx *ctx, zkmcu_media_endpoint *stream)
{
	char option[64];
	sprintf(option, "cid=%d&streamid=%s", stream->cf.cid, stream->sid);
	return xmpp_ua_mse_send_cmd(ctx->ua, stream->cf.mcu, "dc.del_stream", option, NULL, NULL);
}

static void _add_sink_result(xmpp_ua_t *ua, const char *remote, const char *result, const char *option, void *userdata)
{
	add_endpoint_data *data = (add_endpoint_data *) userdata;
	if (!strcmp(result, "ok"))
	{
		zkmcu_media_endpoint *lsi  = (zkmcu_media_endpoint *)malloc(sizeof(zkmcu_media_endpoint));
		lsi->cf = data->cf;
		mse_option_table params[4];
		params[0].key = strdup("sinkid");
		params[1].key = strdup("server_ip");
		params[2].key = strdup("server_rtp_port");
		params[3].key = strdup("server_rtcp_port");
		_get_mse_option_table(params, 4, option);

		lsi->sid = atoi(params[0].value);
		lsi->server_ip = params[1].value;
		lsi->rtp_port = atoi(params[2].value);
		lsi->rtcp_port = atoi(params[3].value);
		data->cb(data->ctx, lsi, 0, data->userdata);
		for (int i = 0; i < 4; i++)
		{
			free(params[i].key);
			free(params[i].value);
		}
		free(lsi);
	}
	else
	{
		data->cb(data->ctx, NULL, -1, data->userdata);
	}

	free(data);
}

int zkmcu_add_sink(zkmcu_conf_ctx *ctx, const char *mcu, int cid, int sid, on_add_endpoint_result cb, void *user_data)
{
	add_endpoint_data *data = (add_endpoint_data *) malloc(sizeof(add_endpoint_data));
	data->ctx = ctx;
	data->cf.cid = cid;
	data->cf.mcu = strdup(mcu);
	data->cb = cb;
	data->userdata = user_data;
	char option[64];
	sprintf(option, "cid=%d&sid=%d", cid, sid);
	return xmpp_ua_mse_send_cmd(ctx->ua, mcu, "add_sink", option, _add_sink_result, data);
}

int zkmcu_remove_sink(zkmcu_conf_ctx *ctx, zkmcu_media_endpoint *sink)
{
	char option[64];
	sprintf(option, "cid=%d&sinkid=%s", sink->cf.cid, sink->sid);
	return xmpp_ua_mse_send_cmd(ctx->ua, sink->cf.mcu, "del_sink", option, NULL, NULL);
}
