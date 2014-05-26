#include "zksignal.h"
#include <string>
#include <stdlib.h>
#include <windows.h>

#define  XMPP_NS_MUC_ROOM_INFO "http://jabber.org/protocol/disco#info"

struct zkmuc_ctx_t {
	xmpp_ua_t *ua;
	
	on_zkmuc_connect_notify conn_cb;
	void *conn_userdata;

	on_zkmuc_receive_invite invite_cb;
	void *invite_data;

	zkmuc_room_cbs room_cbs;/////////
	void *room_data;//////////////

	char *room_id;
	char *room_jid;
	char *room_desc;

	HANDLE _mutex_4_source;
	zkmuc_source_t *head;
	zkmuc_source_t **tail;	
};

zkmuc_ctx_t *zkmuc_ctx_new(xmpp_ua_t *ua)
{
	zkmuc_ctx_t *ctx = (zkmuc_ctx_t *) malloc(sizeof(zkmuc_ctx_t));
	memset(ctx, 0, sizeof(zkmuc_ctx_t));
	ctx->ua = ua;
	ctx->tail = &(ctx->head);
	ctx->_mutex_4_source = CreateMutex(NULL, 0, NULL);
	return ctx;
}

void zkmuc_ctx_delete(zkmuc_ctx_t *ctx)
{
	CloseHandle(ctx->_mutex_4_source);
	free(ctx);
}


static void _zkmuc_conn_handler(xmpp_ua_t *ua, int code, void *userdata)  
{
	zkmuc_ctx_t *ctx = (zkmuc_ctx_t *)userdata;
	ctx->conn_cb(ctx, code, ctx->conn_userdata);
}
void zkmuc_add_login_cb(zkmuc_ctx_t *ctx, on_zkmuc_connect_notify cb, void *userdata)
{
	ctx->conn_cb = cb;
	ctx->conn_userdata = userdata;
	xmpp_ua_conn_handler_add(ctx->ua, _zkmuc_conn_handler, ctx);
}

static int _zkmuc_on_rcv_invite(xmpp_ua_t *ua, xmpp_stanza_t *stanza, void *userdata)
{
	char *ns = xmpp_stanza_get_ns(stanza);
	if (ns != NULL && !strcmp(ns, XMPP_NS_CONFERECE))
	{
		zkmuc_ctx_t *ctx = (zkmuc_ctx_t *)userdata;
		char *from = xmpp_stanza_get_attribute(stanza, "from");
		xmpp_stanza_t *stanza_body = xmpp_stanza_get_child_by_name(stanza, "body");
		if (stanza_body)
		{
			char *room_id = xmpp_stanza_get_text(stanza_body);
			ctx->invite_cb(ctx, from, room_id, ctx->invite_data);
		}
		return 1;
	}
	return 0;
}

void zkmuc_add_invite_cb(zkmuc_ctx_t *ctx, on_zkmuc_receive_invite cb, void *userdata)
{
	ctx->invite_data = userdata;
	ctx->invite_cb = cb;
	xmpp_ua_msg_handler_add(ctx->ua, _zkmuc_on_rcv_invite, ctx);
}

int zkmuc_create_room(zkmuc_ctx_t *ctx, const char *room_id, const char *nick, const char *description, 
	zkmuc_room_cbs *cbs, void *userdata)
{
	ctx->room_desc = strdup(description);
	return zkmuc_enter_room(ctx, room_id, nick, cbs, userdata);
}

static int zkmuc_destroy_room_result(xmpp_ua_t *ua, xmpp_stanza_t *stanza, void *userdata)
{
	return 1;
}

static void _zkmuc_destroy_room(char *room_jid, xmpp_ua_t *ua)
{
	xmpp_stanza_t *iq = xmpp_stanza_new(_xmpp_ctx);
	
	char id[128];
	xmpp_ua_get_unique_string(ua, id);

	xmpp_stanza_set_name(iq, "iq");
	xmpp_stanza_set_id(iq, id);
	xmpp_stanza_set_type(iq, "set");
	xmpp_stanza_set_attribute(iq, "to", room_jid);
	xmpp_stanza_t *query = xmpp_stanza_new(_xmpp_ctx);
	xmpp_stanza_set_name(query, "query");
	xmpp_stanza_set_ns(query, XMPP_NS_MUC_OWNER);

	xmpp_stanza_t *destroy = xmpp_stanza_new(_xmpp_ctx);
	xmpp_stanza_set_name(destroy, "destroy");
	xmpp_stanza_set_attribute(destroy, "jid", room_jid);

	xmpp_stanza_add_child(query, destroy);
	xmpp_stanza_release(destroy);
	xmpp_stanza_add_child(iq, query);
	xmpp_stanza_release(query);
	xmpp_ua_id_handler_add(ua, zkmuc_destroy_room_result, id, NULL);
	xmpp_ua_send(ua, iq);
	xmpp_stanza_release(iq);
}

static int zkmuc_unlock_room_result(xmpp_ua_t *ua, xmpp_stanza_t *stanza, void *userdata)
{
	zkmuc_ctx_t *ctx = (zkmuc_ctx_t *)userdata;
	char *type = xmpp_stanza_get_type(stanza);
	if (!strcmp("result", type)) {
		if (ctx->room_cbs.on_room_result) {
			ctx->room_cbs.on_room_result(ctx, 0, ctx->room_data);
		}
	}
	else {
		_zkmuc_destroy_room(xmpp_stanza_get_attribute(stanza, "from"), ctx->ua);
		if (ctx->room_cbs.on_room_result) {
			ctx->room_cbs.on_room_result(ctx, -1, ctx->room_data);	
		}
	}
	return 1;
}
static void zkmuc_unlock_room(zkmuc_ctx_t *ctx)
{
	xmpp_stanza_t *iq = xmpp_stanza_new(_xmpp_ctx);
	char id[128];
	xmpp_ua_get_unique_string(ctx->ua, id);

	xmpp_stanza_set_name(iq, "iq");
	xmpp_stanza_set_id(iq, id);
	xmpp_stanza_set_type(iq, "set");
	xmpp_stanza_set_attribute(iq, "to", ctx->room_id);

	xmpp_stanza_t *query = xmpp_stanza_new(_xmpp_ctx);
	xmpp_stanza_set_name(query, "query");
	xmpp_stanza_set_ns(query, XMPP_NS_MUC_OWNER);

	xmpp_stanza_t *x = xmpp_stanza_new(_xmpp_ctx);
	xmpp_stanza_set_name(x, "x");
	xmpp_stanza_set_ns(x, XMPP_NS_X);
	xmpp_stanza_set_type(x, "submit");

	xmpp_stanza_t *field = xmpp_stanza_new(_xmpp_ctx);
	xmpp_stanza_set_name(field, "field");
	xmpp_stanza_set_attribute(field, "var", "muc#roomconfig_roomname");

	xmpp_stanza_t *stanza_value = xmpp_stanza_new(_xmpp_ctx);
	xmpp_stanza_set_name(stanza_value, "value");

	xmpp_stanza_t *stanza_text = xmpp_stanza_new(_xmpp_ctx);
	xmpp_stanza_set_text(stanza_text, ctx->room_desc);
	xmpp_stanza_add_child(stanza_value, stanza_text);
	xmpp_stanza_release(stanza_text);

	xmpp_stanza_add_child(field, stanza_value);
	xmpp_stanza_release(stanza_value);

	xmpp_stanza_add_child(x, field);
	xmpp_stanza_release(field);

	xmpp_stanza_add_child(query, x);
	xmpp_stanza_release(x);
	xmpp_stanza_add_child(iq, query);
	xmpp_stanza_release(query);

	xmpp_ua_id_handler_add(ctx->ua, zkmuc_unlock_room_result, id, ctx);
	xmpp_ua_send(ctx->ua, iq);
	xmpp_stanza_release(iq);
}

static int zkmuc_room_presence_handler(xmpp_ua_t *ua, xmpp_stanza_t *stanza, void *userdata)
{
	zkmuc_ctx_t *ctx = (zkmuc_ctx_t *)userdata;
	char *from = xmpp_stanza_get_attribute(stanza, "from");
	if (!from)
	{
		return 0;
	}
	char *type = xmpp_stanza_get_type(stanza);
	if (!type)
	{
		if (!strcmp(from, ctx->room_jid))
		{
			xmpp_stanza_t *stanza_x = xmpp_stanza_get_child_by_name(stanza, "x");
			if (stanza_x)
			{
				xmpp_stanza_t *stanza_status = xmpp_stanza_get_child_by_name(stanza_x, "status");
				while (stanza_status)
				{
					if (!strcmp("status", xmpp_stanza_get_name(stanza_status)))
					{
						char *code = xmpp_stanza_get_attribute(stanza_status, "code");
						if (!strcmp(code, "201"))
						{
							zkmuc_unlock_room(ctx);
							return 1;
						}
					}
					stanza_status = xmpp_stanza_get_next(stanza_status);
				}

				if (!stanza_status && ctx->room_cbs.on_room_result)
				{
					ctx->room_cbs.on_room_result(ctx, 0, ctx->room_data);
					return 1;
				}
			}
		}
		else
		{
			xmpp_stanza_t *stanza_x = xmpp_stanza_get_child_by_name(stanza, "x");
			if (stanza_x)
			{
				xmpp_stanza_t *stanza_item = xmpp_stanza_get_child_by_name(stanza_x, "item");
				if (stanza_item){
					char *remote_jid = xmpp_stanza_get_attribute(stanza_item, "jid");
					if (remote_jid) {
						if (ctx->room_cbs.on_someone_enter_room)
						{
							ctx->room_cbs.on_someone_enter_room(ctx, remote_jid, ctx->room_data);
							return 1;
						}
					}
				}
			}			
		}
	}	
	else if (!strcmp(type, "unavailable"))
	{
		xmpp_stanza_t *stanza_x = xmpp_stanza_get_child_by_name(stanza, "x");
		if (stanza_x)
		{
			xmpp_stanza_t *stanza_item = xmpp_stanza_get_child_by_name(stanza_x, "item");
			if (stanza_item){
				char *jid = xmpp_stanza_get_attribute(stanza_item, "jid");
				if (jid ) {
					if (ctx->room_cbs.on_someone_leave_room)
					{
						ctx->room_cbs.on_someone_leave_room(ctx, jid, ctx->room_data);
						return 1;
					}
				}
			}
		}
	}
	return 0;	
}

static int zkmuc_group_msg_handler(xmpp_ua_t *ua, xmpp_stanza_t *stanza, void *userdata)
{
	char *type = xmpp_stanza_get_type(stanza);
	if (type && !strcmp(type, "groupchat"))
	{
		zkmuc_ctx_t *ctx = (zkmuc_ctx_t *)userdata;
	//	char *from = xmpp_stanza_get_attribute(stanza, "from");
		xmpp_stanza_t *stanza_body = xmpp_stanza_get_child_by_name(stanza, "zonekey");
		if (!stanza_body)
		{
			return 0;
		}
		xmpp_stanza_t *stanza_jid = xmpp_stanza_get_child_by_name(stanza_body, "jid");
		char *jid_value = xmpp_stanza_get_text(stanza_jid);
		char *text = xmpp_stanza_get_text(stanza_body);
		if (text && ctx->room_cbs.on_broadcast_message)
		{
			ctx->room_cbs.on_broadcast_message(ctx, /*from*/jid_value, text, ctx->room_data);
		}
		xmpp_free(_xmpp_ctx, jid_value);
		xmpp_free(_xmpp_ctx, text);
		return 1;
	}
	
	return 0;
}

struct zkmuc_source_t
{
	char *description;
	char *mcu;
	int cid;
	int sid;
	zkmuc_source_t *next;
};

static int _zkmuc_source_query(xmpp_ua_t *ua, xmpp_stanza_t *stanza, void *userdata)
{
	zkmuc_ctx_t *ctx = (zkmuc_ctx_t *)userdata;
	char *from = xmpp_stanza_get_attribute(stanza, "from");
	char *id = xmpp_stanza_get_id(stanza);
	if (id == NULL)
	{
		return 0;
	}
	xmpp_stanza_t *x = xmpp_stanza_get_child_by_name(stanza, "x");
	if(x && xmpp_stanza_get_ns(x) && !strcmp(XMPP_NS_SOURCE, xmpp_stanza_get_ns(x)))
	{
		char *action = xmpp_stanza_get_attribute(x, "action");
		if (!strcmp("query", action))	
		{
			xmpp_stanza_t *message = xmpp_stanza_new(_xmpp_ctx);
			xmpp_stanza_set_name(message, "message");
			xmpp_stanza_set_attribute(message, "to", from);
			xmpp_stanza_set_id(message, id);

			xmpp_stanza_t *result_x = xmpp_stanza_new(_xmpp_ctx);
			xmpp_stanza_set_name(result_x, "x");
			xmpp_stanza_set_ns(result_x, XMPP_NS_SOURCE);
			xmpp_stanza_set_attribute(result_x, "action", "result");

			WaitForSingleObject(ctx->_mutex_4_source, INFINITE);
			zkmuc_source_t *item = ctx->head;
			while (item)
			{
				xmpp_stanza_t *stanza_item = xmpp_stanza_new(_xmpp_ctx);
				xmpp_stanza_set_name(stanza_item, "item");
				char buf[32];
				xmpp_stanza_set_attribute(stanza_item, "cid", itoa(item->cid, buf, 10));
				xmpp_stanza_set_attribute(stanza_item, "sid", itoa(item->sid, buf, 10));
				xmpp_stanza_set_attribute(stanza_item, "desc", item->description);
				xmpp_stanza_set_attribute(stanza_item, "mcu", item->mcu);
				xmpp_stanza_add_child(result_x, stanza_item);
				xmpp_stanza_release(stanza_item);

				item = item->next;
			}
			ReleaseMutex(ctx->_mutex_4_source);
			xmpp_stanza_add_child(message, result_x);
			xmpp_stanza_release(result_x);
			xmpp_ua_send(ctx->ua, message);
			xmpp_stanza_release(message);

			return 1;
		}
	}
	return 0;
}
int zkmuc_enter_room(zkmuc_ctx_t *ctx, const char *room_id, const char *nick, zkmuc_room_cbs *cbs, void *user_data)
{
	char room_jid[256];
	ctx->room_id = strdup(room_id);
	sprintf(room_jid, "%s/%s", room_id, nick);
	ctx->room_jid = strdup(room_jid);
	ctx->room_cbs = *cbs;///////
	ctx->room_data = user_data;///////

	xmpp_ua_presence_handler_add(ctx->ua, zkmuc_room_presence_handler, ctx);
	xmpp_ua_msg_handler_add(ctx->ua, zkmuc_group_msg_handler, ctx);
	xmpp_ua_msg_handler_add(ctx->ua, _zkmuc_source_query, ctx);
	xmpp_stanza_t *prensece = xmpp_stanza_new(_xmpp_ctx);
	xmpp_stanza_set_name(prensece, "presence");
	xmpp_stanza_set_attribute(prensece, "to", ctx->room_jid);

	xmpp_stanza_t *x = xmpp_stanza_new(_xmpp_ctx);
	xmpp_stanza_set_name(x, "x");
	xmpp_stanza_set_ns(x, XMPP_NS_MUC);
	xmpp_stanza_add_child(prensece, x);
	xmpp_stanza_release(x);

	xmpp_ua_send(ctx->ua, prensece);
	xmpp_stanza_release(prensece);

	return 0;
}

int zkmuc_destroy_room(zkmuc_ctx_t *ctx)
{
	_zkmuc_destroy_room(ctx->room_id, ctx->ua);
	return 0;
}

void zkmuc_leave_room(zkmuc_ctx_t *ctx)
{
	xmpp_stanza_t *prensece = xmpp_stanza_new(_xmpp_ctx);
	xmpp_stanza_set_name(prensece, "presence");
	xmpp_stanza_set_attribute(prensece, "to", ctx->room_jid);
	xmpp_stanza_set_type(prensece, "unavailable");
	xmpp_ua_send(ctx->ua, prensece);
	xmpp_stanza_release(prensece);
}

struct room_info_data 
{
	on_get_room_description cb;
	zkmuc_ctx_t * ctx;
	void *user_data;
};

static int _on_room_info(xmpp_ua_t *ua, xmpp_stanza_t * const stanza, void * const userdata)
{
	room_info_data *info_data = (room_info_data *)userdata;
	char *type = xmpp_stanza_get_type(stanza);
	if (strcmp(type, "result"))
	{
		info_data->cb(info_data->ctx, NULL, -2, info_data->user_data);
		free(info_data);
		return 1;
	}

	xmpp_stanza_t *stanza_query = xmpp_stanza_get_child_by_name(stanza, "query");
	if (stanza_query)
	{
		xmpp_stanza_t *stanza_identity = xmpp_stanza_get_child_by_name(stanza_query, "identity");
		if (stanza_identity)
		{
			char *room_desc = xmpp_stanza_get_attribute(stanza_identity, "name");
			info_data->cb(info_data->ctx, room_desc, 0, info_data->user_data);
			free(info_data);
			return 1;
		}
	}

	info_data->cb(info_data->ctx, NULL, -1, info_data->user_data);
	free(info_data);
	return 1;
}

int zkmuc_get_room_description(zkmuc_ctx_t *ctx, const char *room_id, on_get_room_description cb, void *user_data)
{
	char iq_id[128];
	xmpp_ua_get_unique_string(ctx->ua, iq_id);
	xmpp_stanza_t *stanza_iq = xmpp_stanza_new(_xmpp_ctx);
	xmpp_stanza_set_name(stanza_iq, "iq");
	xmpp_stanza_set_attribute(stanza_iq, "to", room_id);
	xmpp_stanza_set_attribute(stanza_iq, "id", iq_id);
	xmpp_stanza_set_type(stanza_iq, "get");

	xmpp_stanza_t *stanza_query = xmpp_stanza_new(_xmpp_ctx);
	xmpp_stanza_set_name(stanza_query, "query");
	xmpp_stanza_set_ns(stanza_query, XMPP_NS_MUC_ROOM_INFO);
	xmpp_stanza_add_child(stanza_iq, stanza_query);
	xmpp_stanza_release(stanza_query);

	room_info_data *info_data = (room_info_data *)malloc(sizeof(room_info_data));
	info_data->cb = cb;
	info_data->ctx = ctx;
	info_data->user_data = user_data;
	xmpp_ua_id_handler_add(ctx->ua, _on_room_info, iq_id, info_data);
	xmpp_ua_send(ctx->ua, stanza_iq);
	xmpp_stanza_release(stanza_iq);
	return 0;
}

int zkmuc_broadcast_message(zkmuc_ctx_t *ctx, const char *msg)
{
	xmpp_stanza_t *stanza_msg = xmpp_stanza_new(_xmpp_ctx);
	xmpp_stanza_set_name(stanza_msg, "message");
	xmpp_stanza_set_attribute(stanza_msg, "to", ctx->room_id);
	xmpp_stanza_set_type(stanza_msg, "groupchat");

	xmpp_stanza_t *stanza_body = xmpp_stanza_new(_xmpp_ctx);
	xmpp_stanza_set_name(stanza_body, "zonekey");
	xmpp_stanza_t *stanza_jid = xmpp_stanza_new(_xmpp_ctx);
	xmpp_stanza_set_name(stanza_jid, "jid");
	xmpp_stanza_t *stanza_jid_value = xmpp_stanza_new(_xmpp_ctx);
	xmpp_stanza_set_text(stanza_jid_value, xmpp_ua_get_jid(ctx->ua));
	xmpp_stanza_add_child(stanza_jid, stanza_jid_value);
	xmpp_stanza_release(stanza_jid_value);
	xmpp_stanza_add_child(stanza_body, stanza_jid);
	xmpp_stanza_release(stanza_jid);
	xmpp_stanza_t *stanza_txt = xmpp_stanza_new(_xmpp_ctx);
	xmpp_stanza_set_text(stanza_txt, msg);
	xmpp_stanza_add_child(stanza_body, stanza_txt);
	xmpp_stanza_release(stanza_txt);
	xmpp_stanza_add_child(stanza_msg, stanza_body);
	xmpp_stanza_release(stanza_body);
	xmpp_ua_send(ctx->ua, stanza_msg);
	xmpp_stanza_release(stanza_msg);
	return 0;
}

zkmuc_source_t * zkmuc_add_source(zkmuc_ctx_t *ctx, const char *mcu, int cid, int sid, const char *description)
{
	zkmuc_source_t *source = (zkmuc_source_t *)malloc(sizeof(zkmuc_source_t));
	source->mcu = strdup(mcu);
	source->cid = cid;
	source->sid = sid;
	if (description)
		source->description = strdup(description);
	else
		source->description = NULL;
	source->next = NULL;
	WaitForSingleObject(ctx->_mutex_4_source, INFINITE);
	*(ctx->tail) = source;
	ctx->tail = &(source->next);
	ReleaseMutex(ctx->_mutex_4_source);

	return 0;
}

int zkmuc_remove_source(zkmuc_ctx_t *ctx, zkmuc_source_t *source)
{
	WaitForSingleObject(ctx->_mutex_4_source, INFINITE);
	zkmuc_source_t **current = &(ctx->head);
	while (*current != source && *current)
	{
		current = &((*current)->next);
	}
	if (*current == NULL)
	{
		return -1;
	}
	current = &((*current)->next);
	if (source->description)
	{
		free(source->description);
	}
	free(source);
	ReleaseMutex(ctx->_mutex_4_source);
	return 0;
}

zkmuc_source_t *zkmuc_get_next_remote_source(zkmuc_source_t *remote_source)
{
	return remote_source->next;
}
const char *zkmuc_get_remote_description(zkmuc_source_t *remote_source)
{
	return remote_source->description;
}
const char *zkmuc_get_remote_mcu(zkmuc_source_t *remote_source)
{
	return remote_source->mcu;
}
int zkmuc_get_remote_cid(zkmuc_source_t *remote_source)
{
	return remote_source->cid;
}
int zkmuc_get_remote_sid(zkmuc_source_t *remote_source)
{
	return remote_source->sid;
}

struct query_source_data
{
	zkmuc_ctx_t *ctx;
	on_remote_source cb;
	void *userdata;
};

static void _zkmuc_free_all_remote_source(zkmuc_source_t *first_remote_source)
{
	zkmuc_source_t *head = first_remote_source;
	while (head)
	{
		zkmuc_source_t *next = head->next;
		if (head->description)
			free(head->description);	
		if (head->mcu)
		{
			free(head->mcu);
		}
		free(head);
		head = next;
	}	
}
static int _zkmuc_on_source_query(xmpp_ua_t *ua, xmpp_stanza_t *stanza, void *userdata)
{
	query_source_data *data = (query_source_data *)userdata;
	char *from = xmpp_stanza_get_attribute(stanza, "from");
	xmpp_stanza_t *x = xmpp_stanza_get_child_by_name(stanza, "x");
	xmpp_stanza_t *item = xmpp_stanza_get_children(x);
	zkmuc_source_t *head = NULL;
	zkmuc_source_t **current = &head;
	while (item)
	{
		if (!strcmp("item", xmpp_stanza_get_name(item)))
		{
			*current = (zkmuc_source_t *)malloc(sizeof(zkmuc_source_t));

			(*current)->cid = atoi(xmpp_stanza_get_attribute(item, "cid"));
			(*current)->sid = atoi(xmpp_stanza_get_attribute(item, "sid"));
			(*current)->description = strdup(xmpp_stanza_get_attribute(item, "desc"));
			(*current)->mcu = strdup(xmpp_stanza_get_attribute(item, "mcu"));
			current = &(*current)->next;
		}
		item = xmpp_stanza_get_next(item);
	}
	*current = NULL;
	data->cb(data->ctx, from, data->userdata, head);
	_zkmuc_free_all_remote_source(head);
	free(data);
	return 1;
}

int zkmuc_get_remote_source(zkmuc_ctx_t *ctx, const char *remote_id, on_remote_source cb, void *userdata)
{
	char id[128];
	xmpp_ua_get_unique_string(ctx->ua, id);
	query_source_data *data = (query_source_data *)malloc(sizeof(query_source_data));
	data->cb = cb;
	data->ctx = ctx;
	data->userdata = userdata;
	xmpp_ua_id_handler_add(ctx->ua, _zkmuc_on_source_query, id, data);
	xmpp_stanza_t *message = xmpp_stanza_new(_xmpp_ctx);
	xmpp_stanza_set_name(message, "message");
	xmpp_stanza_set_attribute(message, "to", remote_id);
	xmpp_stanza_set_id(message, id);
	xmpp_stanza_t *x = xmpp_stanza_new(_xmpp_ctx);
	xmpp_stanza_set_name(x, "x");
	xmpp_stanza_set_ns(x, XMPP_NS_SOURCE);
	xmpp_stanza_set_attribute(x, "action", "query");
	xmpp_stanza_add_child(message, x);
	xmpp_stanza_release(x);
	xmpp_ua_send(ctx->ua, message);
	xmpp_stanza_release(message);

	return 0;
}

int zkmuc_invite(zkmuc_ctx_t *ctx, const char *remote_id)
{
	xmpp_stanza_t *stanza_msg = xmpp_stanza_new(_xmpp_ctx);
	xmpp_stanza_set_name(stanza_msg, "message");
	xmpp_stanza_set_attribute(stanza_msg, "to", remote_id);
	xmpp_stanza_set_ns(stanza_msg, XMPP_NS_CONFERECE);

	xmpp_stanza_t *stanza_body = xmpp_stanza_new(_xmpp_ctx);
	xmpp_stanza_set_name(stanza_body, "body");

	xmpp_stanza_t *stanza_txt = xmpp_stanza_new(_xmpp_ctx);
	xmpp_stanza_set_text(stanza_txt, ctx->room_id);

	xmpp_stanza_add_child(stanza_body,stanza_txt);
	xmpp_stanza_release(stanza_txt);
	xmpp_stanza_add_child(stanza_msg, stanza_body);
	xmpp_stanza_release(stanza_body);

	xmpp_ua_send(ctx->ua, stanza_msg);
	xmpp_stanza_release(stanza_msg);
	return 0;
}