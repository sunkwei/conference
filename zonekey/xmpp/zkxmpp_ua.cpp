#include "zkxmpp_ua.h"
#include <windows.h>
#include <map>
#include <string>
#include <time.h>

struct xmpp_ua_cb_data 
{
	xmpp_ua_cb cb;
	void *userdata;
};
struct xmpp_ua_cb_list
{
	xmpp_ua_cb_data data;
	xmpp_ua_cb_list *next;
};

struct xmpp_ua_conn_cb_list
{
	xmpp_ua_conn_handler cb;
	void *userdata;
	xmpp_ua_conn_cb_list *next;
};

struct mse_cmd_data
{
	xmpp_ua_mse_receive_rsp cb;
	void *userdata;
};
typedef std::map<std::string, xmpp_ua_cb_data> map_id_cb;
typedef std::map<int, mse_cmd_data> mse_cmd_map;

struct xmpp_ua_t 
{
	xmpp_conn_t *conn;
	HANDLE mutex_4_ua;
	xmpp_ua_conn_cb_list *conn_head;
	xmpp_ua_conn_cb_list **conn_tail;
	xmpp_ua_cb_list *msg_head;
	xmpp_ua_cb_list **msg_tail;
	xmpp_ua_cb_list *presence_head;
	xmpp_ua_cb_list **presence_tail;
	xmpp_ua_cb_list *iq_head;
	xmpp_ua_cb_list **iq_tail;
	map_id_cb id_cb_map;
	mse_cmd_map mse_map;
};

xmpp_ctx_t *_xmpp_ctx;
static HANDLE _xmpp_thread;
static HANDLE _mutex_4_conn;
typedef std::map<xmpp_conn_t *, xmpp_ua_t *> conn_ua_map;
static conn_ua_map  _conn_map;
static HANDLE _mutex_4_id;
static int _unique_id;

static DWORD WINAPI _work_thread(LPVOID lpThreadParameter)
{
	xmpp_run(_xmpp_ctx);
	return 0;
}

void xmpp_ua_init(void)
{
	xmpp_initialize();
	_xmpp_ctx = xmpp_ctx_new(NULL, NULL);
	_mutex_4_conn = CreateMutex(NULL, 0, NULL);
	_mutex_4_id = CreateMutex(NULL, 0, NULL);
	_xmpp_thread = CreateThread(NULL, 0, _work_thread, NULL, 0, NULL);
}

void xmpp_ua_uninit(void)
{
	xmpp_stop(_xmpp_ctx);
	WaitForSingleObject(_xmpp_thread, INFINITE);
	CloseHandle(_xmpp_thread);
	CloseHandle(_mutex_4_conn);
	CloseHandle(_mutex_4_id);
	xmpp_ctx_free(_xmpp_ctx);
	xmpp_shutdown();
}

static  int _cb_mesage(xmpp_conn_t * const conn,
	xmpp_stanza_t * const stanza,
	void * const userdata)
{
	WaitForSingleObject(_mutex_4_conn, INFINITE);
	conn_ua_map::iterator iter = _conn_map.find(conn);
	if (iter == _conn_map.end())
	{
		ReleaseMutex(_mutex_4_conn);
		return 1;
	}
	xmpp_ua_t *ua= iter->second;
	ReleaseMutex(_mutex_4_conn);

	char *id = xmpp_stanza_get_id(stanza);
	WaitForSingleObject(ua->mutex_4_ua, INFINITE);
	if (id)
	{
		std::string string_id(id);
		map_id_cb::iterator cb_iter = ua->id_cb_map.find(string_id);
		if (cb_iter != ua->id_cb_map.end())
		{
			cb_iter->second.cb(ua, stanza, cb_iter->second.userdata);
			ua->id_cb_map.erase(cb_iter);
			ReleaseMutex(ua->mutex_4_ua);
			return 1;
		}
	}
	xmpp_ua_cb_list *current = ua->msg_head;
	while (current)
	{
		if (current->data.cb(ua, stanza, current->data.userdata))
		{
			ReleaseMutex(ua->mutex_4_ua);
			return 1;
		}
		current = current->next;
	}
	ReleaseMutex(ua->mutex_4_ua);
	return 1;
}

static  int _cb_presence(xmpp_conn_t * const conn,
	xmpp_stanza_t * const stanza,
	void * const userdata)
{
	WaitForSingleObject(_mutex_4_conn, INFINITE);
	conn_ua_map::iterator iter = _conn_map.find(conn);
	if (iter == _conn_map.end())
	{
		ReleaseMutex(_mutex_4_conn);
		return 1;
	}
	xmpp_ua_t *ua= iter->second;
	ReleaseMutex(_mutex_4_conn);
	char *id = xmpp_stanza_get_id(stanza);
	WaitForSingleObject(ua->mutex_4_ua, INFINITE);
	if (id)
	{
		std::string string_id(id);
		map_id_cb::iterator cb_iter = ua->id_cb_map.find(string_id);
		if (cb_iter != ua->id_cb_map.end())
		{
			cb_iter->second.cb(ua, stanza, cb_iter->second.userdata);
			ua->id_cb_map.erase(cb_iter);
			ReleaseMutex(ua->mutex_4_ua);
			return 1;
		}
	}
	xmpp_ua_cb_list *current = ua->presence_head;
	while (current)
	{
		if (current->data.cb(ua, stanza, current->data.userdata))
		{
			ReleaseMutex(ua->mutex_4_ua);
			return 1;
		}
		current = current->next;
	}

	ReleaseMutex(ua->mutex_4_ua);
	return 1;
}

static void _respond_iq_with_error(xmpp_conn_t *conn, xmpp_stanza_t *stanza, const char *type, const char* condition)
{
	char *id = xmpp_stanza_get_attribute(stanza, "id");
	if (!id)
		return;
	xmpp_stanza_t *response = xmpp_stanza_new(_xmpp_ctx);
	xmpp_stanza_set_name(response, "iq");
	xmpp_stanza_set_attribute(response, "type", "error");
	xmpp_stanza_set_attribute(response, "id", id);

	char *req_from = xmpp_stanza_get_attribute(stanza, "from");
	//当req_from为NULL时, to属性应该设为服务器, 不设应该默认是服务器;
	if (req_from)
		xmpp_stanza_set_attribute(response, "to", req_from);

	xmpp_stanza_t *stanza_error = xmpp_stanza_new(_xmpp_ctx);
	xmpp_stanza_set_name(stanza_error, "error");
	xmpp_stanza_set_attribute(stanza_error, "type", type);

	xmpp_stanza_t *stanza_condition = xmpp_stanza_new(_xmpp_ctx);
	xmpp_stanza_set_name(stanza_condition, condition);
	xmpp_stanza_set_ns(stanza_condition, XMPP_NS_STANZA);

	xmpp_stanza_add_child(stanza_error, stanza_condition);
	xmpp_stanza_add_child(response, stanza_error);

	xmpp_stanza_release(stanza_condition);
	xmpp_stanza_release(stanza_error);

	xmpp_send(conn, response);
	xmpp_stanza_release(response);
}

static  int _cb_iq(xmpp_conn_t * const conn,
	xmpp_stanza_t * const stanza,
	void * const userdata)
{
	WaitForSingleObject(_mutex_4_conn, INFINITE);
	conn_ua_map::iterator iter = _conn_map.find(conn);
	if (iter == _conn_map.end())
	{
		ReleaseMutex(_mutex_4_conn);
		return 1;
	}
	xmpp_ua_t *ua= iter->second;
	ReleaseMutex(_mutex_4_conn);
	char *id = xmpp_stanza_get_id(stanza);
	WaitForSingleObject(ua->mutex_4_ua, INFINITE);
	if (id)
	{
		std::string string_id(id);
		map_id_cb::iterator cb_iter = ua->id_cb_map.find(string_id);
		if (cb_iter != ua->id_cb_map.end())
		{
			cb_iter->second.cb(ua, stanza, cb_iter->second.userdata);
			ua->id_cb_map.erase(cb_iter);
			ReleaseMutex(ua->mutex_4_ua);
			return 1;
		}
	}
	xmpp_ua_cb_list *current = ua->iq_head;
	while (current)
	{
		if (current->data.cb(ua, stanza, current->data.userdata))
		{
			ReleaseMutex(ua->mutex_4_ua);
			return 1;
		}
		current = current->next;
	}

	_respond_iq_with_error(ua->conn, stanza, "cancel", "service-unavailable");
	ReleaseMutex(ua->mutex_4_ua);
	return 1;
}

void xmpp_ua_id_handler_add(xmpp_ua_t *ua, xmpp_ua_cb cb, char *id, void *userdata)
{
	WaitForSingleObject(ua->mutex_4_ua, INFINITE);
	xmpp_ua_cb_data cb_data = {cb, userdata};
	ua->id_cb_map[std::string(id)] = cb_data;
	ReleaseMutex(ua->mutex_4_ua);
}

int xmpp_ua_get_unique_id(void)
{
	int result;
	WaitForSingleObject(_mutex_4_id, INFINITE);
	++_unique_id;
	result = _unique_id;
	ReleaseMutex(_mutex_4_id);
	return result;
}

void xmpp_ua_get_unique_string(xmpp_ua_t *ua, char buf[128])
{
	unsigned int now = time(0);
	sprintf(buf, "%s_%u_%d", xmpp_conn_get_jid(ua->conn), now, xmpp_ua_get_unique_id());
}

int query_roster(xmpp_ua_t *ua, xmpp_ua_handler cb, void *userdata)
{	
	
	xmpp_stanza_t *stanza_iq = xmpp_stanza_new(_xmpp_ctx);
	xmpp_stanza_set_name(stanza_iq, "iq");
	xmpp_stanza_set_attribute(stanza_iq, "type", "get");

	char id[128];
	xmpp_ua_get_unique_string(ua, id);

	xmpp_stanza_set_attribute(stanza_iq, "id", id);

	xmpp_stanza_t *stanza_query = xmpp_stanza_new(_xmpp_ctx);
	xmpp_stanza_set_name(stanza_query, "query");
	xmpp_stanza_set_ns(stanza_query, XMPP_NS_ROSTER);

	xmpp_stanza_add_child(stanza_iq, stanza_query);
	xmpp_stanza_release(stanza_query);
	xmpp_ua_id_handler_add(ua, cb, id, userdata);
	int result = xmpp_ua_send(ua, stanza_iq);
	xmpp_stanza_release(stanza_iq);
	return result;
}

int xmpp_ua_present(xmpp_ua_t *ua)
{
	xmpp_stanza_t *stanza_presence = xmpp_stanza_new(_xmpp_ctx);
	xmpp_stanza_set_name(stanza_presence, "presence");
	int result = xmpp_ua_send(ua, stanza_presence);
	xmpp_stanza_release(stanza_presence);
	return result;
}

static void _on_xmpp_conn(xmpp_conn_t * const conn, 
	const xmpp_conn_event_t event,
	const int error,
	xmpp_stream_error_t * const stream_error,
	void * const userdata)
{
	WaitForSingleObject(_mutex_4_conn, INFINITE);
	conn_ua_map::iterator iter = _conn_map.find(conn);
	if (iter == _conn_map.end())
	{
		ReleaseMutex(_mutex_4_conn);
		return;
	}
	xmpp_ua_t *ua= iter->second;
	ReleaseMutex(_mutex_4_conn);

	if (event == XMPP_CONN_CONNECT)
	{
		xmpp_handler_add(conn, _cb_mesage, NULL, "message", NULL, NULL);
		xmpp_handler_add(conn, _cb_presence, NULL, "presence", NULL, NULL);
		xmpp_handler_add(conn, _cb_iq, NULL, "iq", NULL, NULL);	
		WaitForSingleObject(ua->mutex_4_ua, INFINITE);
		xmpp_ua_conn_cb_list *current = ua->conn_head;
		while (current)
		{
			current->cb(ua, 0, current->userdata);
			current = current->next;
		}
		ReleaseMutex(ua->mutex_4_ua);
	}
	else
	{
		WaitForSingleObject(_mutex_4_conn, INFINITE);
		_conn_map.erase(iter);
		ReleaseMutex(_mutex_4_conn);
		
		WaitForSingleObject(ua->mutex_4_ua, INFINITE);
		xmpp_ua_conn_cb_list *current = ua->conn_head;
		while (current)
		{
			current->cb(ua, -1, current->userdata);
			current = current->next;
		}
		ua->conn = NULL;
		ReleaseMutex(ua->mutex_4_ua);
	}
	
}

static int _zkmse_receive_rsp(xmpp_ua_t *ua, xmpp_stanza_t *stanza, void *userdata)
{
	char *type = xmpp_stanza_get_type(stanza);
	if (!type || strcmp(type, "chat"))
		return 0;
	char *from = xmpp_stanza_get_attribute(stanza, "from");
	if (!from)
		return 0;
	xmpp_stanza_t *body = xmpp_stanza_get_child_by_name(stanza, "body");
	if (!body)
		return 0;
	char *str_body = xmpp_stanza_get_text(body);
	if (!str_body)
		return 0;
	char cmd_type[16];
	char result[128];
	char *option = (char *)malloc(strlen(str_body));
	int msg_id;

	int rc = sscanf(str_body, "%15[^|]|%d|%*[^|]|%127[^|]|%[^\0]",  cmd_type, &msg_id, result, option);
	if (rc < 4)
	{
		free(option);
		return 0;
	}
	if (!strcmp(cmd_type, "AckCmd"))
	{
		WaitForSingleObject(ua->mutex_4_ua, INFINITE);
		mse_cmd_map::iterator iter = ua->mse_map.find(msg_id);
		if (iter == ua->mse_map.end())
		{
			free(option);
			ReleaseMutex(ua->mutex_4_ua);
			return 0;
		}
		mse_cmd_data data = iter->second;
		data.cb(ua, from, result, option, data.userdata);
		free(option);
		ReleaseMutex(ua->mutex_4_ua);
		return 1;
	}

	free(option);
	return 0;
}
xmpp_ua_t *xmpp_ua_new(void)
{
	xmpp_ua_t *ua = new xmpp_ua_t;
	ua->mutex_4_ua = CreateMutex(NULL, 0, NULL);
	ua->conn = NULL;
	ua-> conn_head = NULL;
	ua->conn_tail = &(ua->conn_head);
	ua->msg_head = NULL;
	ua->msg_tail = &(ua->msg_head);
	xmpp_ua_msg_handler_add(ua, _zkmse_receive_rsp, ua);
	ua->presence_head = NULL;
	ua->presence_tail = &(ua->presence_head);
	ua->iq_head = NULL;
	ua->iq_tail = &(ua->iq_head);

	return ua;
}

void xmpp_ua_delete(xmpp_ua_t *ua)
{
	xmpp_ua_conn_cb_list *conn_cb_list = ua->conn_head;
	while (conn_cb_list)
	{
		xmpp_ua_conn_cb_list *remove = conn_cb_list;
		conn_cb_list = conn_cb_list->next;
		free(remove);
	}
	xmpp_ua_cb_list *msg_list = ua->msg_head;
	while (msg_list)
	{
		xmpp_ua_cb_list *remove = msg_list;
		msg_list = msg_list->next;
		free(remove);
	}
	xmpp_ua_cb_list *presence_list = ua->presence_head;
	while (presence_list)
	{
		xmpp_ua_cb_list *remove = presence_list;
		presence_list = presence_list->next;
		free(remove);
	}
	
	xmpp_ua_cb_list *iq_list = ua->iq_head;
	while (iq_list)
	{
		xmpp_ua_cb_list *remove = iq_list;
		iq_list = iq_list->next;
		free(remove);
	}
	CloseHandle(ua->mutex_4_ua);
	delete ua;
}

void xmpp_ua_conn_handler_add(xmpp_ua_t *ua, xmpp_ua_conn_handler cb, void *userdata)
{
	WaitForSingleObject(ua->mutex_4_ua, INFINITE);
	(*ua->conn_tail) = (xmpp_ua_conn_cb_list *)malloc(sizeof(xmpp_ua_conn_cb_list));
	(*ua->conn_tail)->cb = cb;
	(*ua->conn_tail)->userdata = userdata;
	(*ua->conn_tail)->next = NULL;
	ua->conn_tail = &((*ua->conn_tail)->next);
	ReleaseMutex(ua->mutex_4_ua);
}

void xmpp_ua_conn_handler_remove(xmpp_ua_t *ua, xmpp_ua_conn_handler cb)
{
	WaitForSingleObject(ua->mutex_4_ua, INFINITE);
	xmpp_ua_conn_cb_list **current =&(ua->conn_head);
	while (*current != NULL)
	{
		if ((*current)->cb == cb)
		{
			xmpp_ua_conn_cb_list *remove = *current;
			*current = (*current)->next;
			free(remove);
			ReleaseMutex(ua->mutex_4_ua);
			return;
		}
		current = &((*current)->next);
	}
	ReleaseMutex(ua->mutex_4_ua);
}
void *xmpp_ua_conn_get_userdata(xmpp_ua_t *ua, xmpp_ua_conn_handler cb)
{
	WaitForSingleObject(ua->mutex_4_ua, INFINITE);
	xmpp_ua_conn_cb_list **current =&(ua->conn_head);
	while (*current != NULL)
	{
		if ((*current)->cb == cb)
		{
			ReleaseMutex(ua->mutex_4_ua);
			return (*current)->userdata;
		}
		current = &((*current)->next);
	}
	
	ReleaseMutex(ua->mutex_4_ua);
	return NULL;
}

int xmpp_ua_login(xmpp_ua_t * ua, const char *id, const char *pwd)
{
	WaitForSingleObject(ua->mutex_4_ua, INFINITE);
	ua->conn = xmpp_conn_new(_xmpp_ctx);
	xmpp_conn_set_jid(ua->conn, id);
	xmpp_conn_set_pass(ua->conn, pwd);
	int rc = xmpp_connect_client_ex(ua->conn, NULL, NULL, _on_xmpp_conn, ua);
	if (rc != 0) {
		xmpp_conn_release(ua->conn);
		ReleaseMutex(ua->mutex_4_ua);
		return -1;
	}
	ReleaseMutex(ua->mutex_4_ua);

	WaitForSingleObject(_mutex_4_conn, INFINITE);
	_conn_map[ua->conn] = ua;
	ReleaseMutex(_mutex_4_conn);
	return 0;
}
void  xmpp_ua_logout(xmpp_ua_t *ua)
{
	WaitForSingleObject(_mutex_4_conn, INFINITE);
	conn_ua_map::iterator iter = _conn_map.find(ua->conn);
	if (iter == _conn_map.end())
	{
		ReleaseMutex(_mutex_4_conn);
		return;
	}
	xmpp_disconnect(ua->conn);
	_conn_map.erase(iter);
	ReleaseMutex(_mutex_4_conn);
	WaitForSingleObject(ua->mutex_4_ua, INFINITE);
	ua->conn = NULL;
	ReleaseMutex(ua->mutex_4_ua);
}

void xmpp_ua_msg_handler_add(xmpp_ua_t *ua, xmpp_ua_handler msg_cb, void *userdata)
{
	WaitForSingleObject(ua->mutex_4_ua, INFINITE);
	(*ua->msg_tail) = (xmpp_ua_cb_list *)malloc(sizeof(xmpp_ua_cb_list));
	(*ua->msg_tail)->data.cb = msg_cb;
	(*ua->msg_tail)->data.userdata = userdata;
	(*ua->msg_tail)->next = NULL;
	ua->msg_tail = &((*ua->msg_tail)->next);
	ReleaseMutex(ua->mutex_4_ua);
}
void xmpp_ua_iq_handler_add(xmpp_ua_t *ua, xmpp_ua_handler iq_cb, void *userdata)
{
	WaitForSingleObject(ua->mutex_4_ua, INFINITE);
	(*ua->iq_tail) = (xmpp_ua_cb_list *)malloc(sizeof(xmpp_ua_cb_list));
	(*ua->iq_tail)->data.cb = iq_cb;
	(*ua->iq_tail)->data.userdata = userdata;
	(*ua->iq_tail)->next = NULL;
	ua->iq_tail = &((*ua->iq_tail)->next);
	ReleaseMutex(ua->mutex_4_ua);
}

void xmpp_ua_presence_handler_add(xmpp_ua_t *ua, xmpp_ua_handler presence_cb, void *userdata)
{
	WaitForSingleObject(ua->mutex_4_ua, INFINITE);
	(*ua->presence_tail) = (xmpp_ua_cb_list *)malloc(sizeof(xmpp_ua_cb_list));
	(*ua->presence_tail)->data.cb = presence_cb;
	(*ua->presence_tail)->data.userdata = userdata;
	(*ua->presence_tail)->next = NULL;
	ua->presence_tail = &((*ua->presence_tail)->next);
	ReleaseMutex(ua->mutex_4_ua);
}

void xmpp_ua_msg_handler_remove(xmpp_ua_t *ua, xmpp_ua_handler cb)
{
	WaitForSingleObject(ua->mutex_4_ua, INFINITE);
	xmpp_ua_cb_list **current =&(ua->msg_head);
	while (*current != NULL)
	{
		if ((*current)->data.cb == cb)
		{
			xmpp_ua_cb_list *remove = *current;
			*current = (*current)->next;
			free(remove);
			ReleaseMutex(ua->mutex_4_ua);
			return;
		}
		current = &((*current)->next);
	}
	ReleaseMutex(ua->mutex_4_ua);
}
void *xmpp_ua_msg_get_userdata(xmpp_ua_t *ua, xmpp_ua_handler cb)
{
	WaitForSingleObject(ua->mutex_4_ua, INFINITE);
	xmpp_ua_cb_list **current =&(ua->msg_head);
	while (*current != NULL)
	{
		if ((*current)->data.cb == cb)
		{
			void *result = (*current)->data.userdata;
			ReleaseMutex(ua->mutex_4_ua);
			return result;
		}
		current = &((*current)->next);
	}
	
	ReleaseMutex(_mutex_4_conn);
	return NULL;
}
void *xmpp_ua_presence_get_userdata(xmpp_ua_t *ua, xmpp_ua_handler cb)
{
	WaitForSingleObject(ua->mutex_4_ua, INFINITE);
	xmpp_ua_cb_list **current =&(ua->presence_head);
	while (*current != NULL)
	{
		if ((*current)->data.cb == cb)
		{
			void *result = (*current)->data.userdata;
			ReleaseMutex(ua->mutex_4_ua);
			return result;
		}
		current = &((*current)->next);
	}
	ReleaseMutex(ua->mutex_4_ua);
	return NULL;
}
void *xmpp_ua_iq_get_userdata(xmpp_ua_t *ua, xmpp_ua_handler cb)
{
	WaitForSingleObject(ua->mutex_4_ua, INFINITE);
	xmpp_ua_cb_list **current =&(ua->iq_head);
	while (*current != NULL)
	{
		if ((*current)->data.cb == cb)
		{
			void *result = (*current)->data.userdata;
			ReleaseMutex(ua->mutex_4_ua);
			return result;
		}
		current = &((*current)->next);
	}
	ReleaseMutex(ua->mutex_4_ua);
	return NULL;
}

void xmpp_ua_presence_handler_remove(xmpp_ua_t *ua, xmpp_ua_handler cb)
{
	WaitForSingleObject(ua->mutex_4_ua, INFINITE);
	xmpp_ua_cb_list **current =&(ua->presence_head);
	while (*current != NULL)
	{
		if ((*current)->data.cb == cb)
		{
			xmpp_ua_cb_list *remove = *current;
			*current = (*current)->next;
			free(remove);
			ReleaseMutex(ua->mutex_4_ua);
			return;
		}
		current = &((*current)->next);
	}
	ReleaseMutex(ua->mutex_4_ua);
}

void xmpp_ua_iq_handler_remove(xmpp_ua_t *ua, xmpp_ua_handler cb)
{
	WaitForSingleObject(ua->mutex_4_ua, INFINITE);
	xmpp_ua_cb_list **current =&(ua->iq_head);
	while (*current != NULL)
	{
		if ((*current)->data.cb == cb)
		{
			xmpp_ua_cb_list *remove = *current;
			*current = (*current)->next;
			free(remove);
			ReleaseMutex(ua->mutex_4_ua);
			return;
		}
		current = &((*current)->next);
	}
	ReleaseMutex(ua->mutex_4_ua);
}

int xmpp_ua_send(xmpp_ua_t *ua, xmpp_stanza_t *stanza)
{
	WaitForSingleObject(ua->mutex_4_ua, INFINITE);
	if (ua->conn == NULL)
	{
		ReleaseMutex(ua->mutex_4_ua);
		return -1;
	}

	xmpp_send(ua->conn, stanza);
	ReleaseMutex(ua->mutex_4_ua);
	return 0;
}

int xmpp_ua_mse_send_cmd(xmpp_ua_t *ua, const char *remote_jid, const char *cmd, const char *option, 
	xmpp_ua_mse_receive_rsp cb, void *userdata)
{
	int cmd_id = -1;
	if (cb)
		cmd_id = xmpp_ua_get_unique_id();
	xmpp_stanza_t *msg = xmpp_stanza_new(_xmpp_ctx);
	xmpp_stanza_set_name(msg, "message");
	xmpp_stanza_set_type(msg, "chat");
	xmpp_stanza_set_attribute(msg, "to", remote_jid);

	xmpp_stanza_t *body = xmpp_stanza_new(_xmpp_ctx);
	xmpp_stanza_set_name(body,"body");

	xmpp_stanza_t *text = xmpp_stanza_new(_xmpp_ctx);
	char *option2;
	xmpp_ua_change_string(option, &option2);
	char *msg_text = (char *)malloc(32+strlen(cmd)+strlen(option2));
	sprintf(msg_text, "ReqCmd|%d|%s|%s", cmd_id, cmd, option2);
	xmpp_stanza_set_text(text, msg_text);
	free(option2);
	free(msg_text);
	
	xmpp_stanza_add_child(body, text);
	xmpp_stanza_release(text);
	xmpp_stanza_add_child(msg, body);
	xmpp_stanza_release(body);

	if (cb)
	{
		mse_cmd_data data = {cb, userdata};
		WaitForSingleObject(ua->mutex_4_ua, INFINITE);
		ua->mse_map[cmd_id] = data;
		ReleaseMutex(ua->mutex_4_ua);
	}
	int rc = xmpp_ua_send(ua, msg);
	xmpp_stanza_release(msg);
	return rc;
}

void xmpp_ua_change_string(const char *str_to_change, char **result)
{
	*result = (char *) malloc(strlen(str_to_change) * 5 +1);//&><替换中&号替换结果最大, 为5个字符;
	char *current_cpy = *result;
	const char *current = str_to_change;
	while (*current)
	{
		if (*current == '&')
		{
			strcpy(current_cpy, "&amp;");
			current_cpy += 5;
		}
		else if (*current == '>')
		{
			strcpy(current_cpy, "&gt;");
			current_cpy += 4;
		}
		else if (*current == '<')
		{
			strcpy(current_cpy, "&lt;");
			current_cpy += 4;
		}
		else
		{
			*current_cpy = *current;
			current_cpy ++;
		}
		current++;
	}
	*current_cpy = '\0';
}

const char *xmpp_ua_get_jid(xmpp_ua_t *ua)
{
	return xmpp_conn_get_jid(ua->conn);
}