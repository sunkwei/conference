#include "../xmpp/zkxmpp_ua.h"

#ifndef _ZK_SIGNAL_HH
#define _ZK_SIGNAL_HH



extern "C" {

	typedef struct zkmuc_ctx_t zkmuc_ctx_t;
	zkmuc_ctx_t *zkmuc_ctx_new(xmpp_ua_t *ua);
	void zkmuc_ctx_delete(zkmuc_ctx_t *ctx);
	
	typedef void (*on_zkmuc_connect_notify)(zkmuc_ctx_t *ctx, int code, void *userdata);
	typedef void (*on_zkmuc_receive_invite)(zkmuc_ctx_t *ctx, const char *invitor_id, const char *room_id, void *userdata);

	void zkmuc_add_login_cb(zkmuc_ctx_t *ctx, on_zkmuc_connect_notify cb, void *userdata);
	void zkmuc_add_invite_cb(zkmuc_ctx_t *ctx, on_zkmuc_receive_invite cb, void *userdata);

	//成功是code为0，其余为错误码;
	//typedef void (*on_create_room_result)(zkmuc_ctx_t *ctx, int code, void *userdata);
	typedef struct zkmuc_room_cbs 
	{
		void (*on_room_result)(zkmuc_ctx_t *ctx,  int code, void *user_data);
		void (*on_someone_enter_room)(zkmuc_ctx_t *ctx,  const char *id, void *user_data);
		//会收到自己退出的消息; 原因可能是主动退出或房间被destroy等;
		void (*on_someone_leave_room)(zkmuc_ctx_t *ctx,  const char *id, void *user_data);
		//NOTE:
		//该函数中的id为on_someone_enter_room中的id;
		//另外注意会收到自己发的broadcast_message;
		void (*on_broadcast_message)(zkmuc_ctx_t *ctx, const char *id, const char *msg, void *user_data);
	} zkmuc_room_cbs;
	//全名格式为room_name@server/nick;
	int zkmuc_create_room(zkmuc_ctx_t *ctx, const char *room_id, const char *nick, const char *description, 
		zkmuc_room_cbs *cbs, void *userdata);
	//destroy_room只有房间的owner(创建者是owner, 也可以由其他owner指定, 指定功能目前没加) 才能调用;
	//房间没人的话服务器端会自动释放房间;
	int zkmuc_destroy_room(zkmuc_ctx_t *ctx);

	//创建者不要调用enter_room, 其已经在房间里了;
	int zkmuc_enter_room(zkmuc_ctx_t *ctx, const char *room_id, const char *nick, zkmuc_room_cbs *cbs, void *user_data);
	void zkmuc_leave_room(zkmuc_ctx_t *ctx);

	typedef void (*on_get_room_description)(zkmuc_ctx_t *ctx, const char *desc, int code, void *user_data);
	int zkmuc_get_room_description(zkmuc_ctx_t *ctx, const char *room_id, on_get_room_description cb, void *user_data);

	int zkmuc_broadcast_message(zkmuc_ctx_t *ctx, const char *msg);

	typedef struct zkmuc_source_t zkmuc_source_t;
	zkmuc_source_t * zkmuc_add_source(zkmuc_ctx_t *ctx, const char *mcu, int cid, int sid, const char *description);
	int zkmuc_remove_source(zkmuc_ctx_t *ctx, zkmuc_source_t *source);

	zkmuc_source_t *zkmuc_get_next_remote_source(zkmuc_source_t *remote_source);
	const char *zkmuc_get_remote_description(zkmuc_source_t *remote_source);
	const char *zkmuc_get_remote_mcu(zkmuc_source_t *remote_source);
	int zkmuc_get_remote_cid(zkmuc_source_t *remote_source);
	int zkmuc_get_remote_sid(zkmuc_source_t *remote_source);

	typedef void (*on_remote_source)(zkmuc_ctx_t *ctx, const char *remote_id, void *user_data, 
		zkmuc_source_t *first_remote_source);
	int zkmuc_get_remote_source(zkmuc_ctx_t *ctx, const char *remote_id, on_remote_source cb, void *userdata);

	//不仅房间创建者可以调用, 被邀请者也可以;
	int zkmuc_invite(zkmuc_ctx_t *ctx, const char *remote_id);
};
#endif