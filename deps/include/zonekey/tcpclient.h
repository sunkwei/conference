#ifndef _tcp_client_hh_
#define _tcp_client_hh_

#include <cc++/socket.h>

#ifdef WIN32
#	ifdef _DLL
#		ifdef LIBMYUTIL_EXPORTS
#			define UTIL_TCPCLIENT_API __declspec(dllexport)
#		else
#			define UTIL_TCPCLIENT_API __declspec(dllimport)
#		endif // 
#	else
#		define UTIL_TCPCLIENT_API
#	endif // _DLL
#else
#	define UTIL_TCPCLIENT_API
#endif // 

namespace util
{

class UTIL_TCPCLIENT_API tcpclient
{
public:
	int m_recv_timeout;
	tcpclient ();
	~tcpclient ();

	int open (const char *url, int conn_timeout);
	int open (const char *url);
	int close ();
	int readline (char *buf, int size);
	int readsome (char *buf, int size);
	int readn (char *buf, int n);
	int writen (const char *data, int len);

	int set_sendbuf(int bufsize);
	int set_recvbuf(int bufsize);

	void set_nonblock(int enable);	// 设置是否阻塞，缺省阻塞模式

private:
	SOCKET m_sock;
};

}; // namespace util

#endif // tcpclient.h
