#ifndef _media_tcpsrv_hh_
#define _media_tcpsrv_hh_

#ifdef __cplusplus
extern "C" {
#endif // c++

typedef void TSContext;
#if !defined WIN32
typedef int SOCKET;
#else
#	include <winsock2.h>
#endif //

/** 每次callback，都会使用独立的线程，使用者可以在这个回调中处理任意时间，如果返回，socket将被关闭
*/
typedef void (*TSNewConnection)(void *opaque, SOCKET sock, int fromport, const char *fromip);

int ts_open (TSContext **handler, int port, const char *bindip, TSNewConnection proc, void *opaque);
int ts_close (TSContext *handler);

struct TSInfo
{
	int thread_worker_total;	//
	int thread_worker_idle;
};

int ts_get_info (TSContext *handler, struct TSInfo *info);

#ifdef __cplusplus
}
#endif // c++

#endif // tcpsrv.h
