#ifndef _zknet_reactor_hh
#define _zknet_reactor_hh

#ifdef __cplusplus
extern "C" {
#endif // c++

typedef struct zk_reactor_t zk_reactor_t;

#define OP_READ  1
#define OP_WRITE  2
#define OP_EXCEPT  4

// 处理函数
typedef void (*zk_fd_handler)(zk_reactor_t *act, int fd, int types, void *opaque);
typedef void (*zk_delay_handler)(zk_reactor_t *act, void *opaque);

// 打开一个selector，内部将启动工作线程
zk_reactor_t *zk_act_open();
void zk_act_close(zk_reactor_t *actor);

// 绑定一个 socket 的操作，如果 types = 0，则结束绑定
int zk_act_bind(zk_reactor_t *act, int fd, int types, zk_fd_handler h, void *opaque);

// 绑定一个延时操作，delay_seconds 为希望延迟的秒数
int zk_act_bind_delay_task(zk_reactor_t *act, double delay_second, zk_delay_handler h, void *opaque);

#ifdef __cplusplus
}
#endif // c=+

#endif // reactor.h
