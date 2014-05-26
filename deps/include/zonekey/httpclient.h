#ifndef _httpclient__hh
#define _httpclient__hh

#include "../libhttpsrv/ehttp.h"
#include "tcpclient.h"

#ifdef WIN32
#	ifdef _DLL
#		ifdef LIBMYUTIL_EXPORTS
#			define UTIL_CONFIG_API __declspec(dllexport)
#		else
#			define UTIL_CONFIG_API __declspec(dllimport)
#		endif // 
#	else
#		define UTIL_CONFIG_API
#	endif // _DLL
#else
#	define UTIL_CONFIG_API
#endif // 

namespace util {
class UTIL_CONFIG_API httpclient
{
public:
	httpclient ();
	~httpclient ();

	/** 打开一个到url指定的http server的链接
	  @param url: 格式必须为 http://<ip>[:<port>]/path, 如果没有指定port,使用 80
	  @return: < 0 失败
	*/
	int open (const char *url);
	int close ();

	/** 发送reqmsg, reqmsg没有必要 isComplete()
	 */
	int send_req (EPower::Message *reqmsg);
	/** 发送任意数据, 一般用于发送大规模body
	 */
	int send (const void *buf, size_t len);

	/** 等待接收msg, 如果msg(包含body)较小, 可能直接接收完成(res isComplete())
	 */
	int recv_res (EPower::Message **res);
	/** 当 recv_res 没有接收完整的 body时, 需要调用这个函数接收剩余数据 residualBodyLength()
	 */
	int recv (void *buf, size_t len);

	/** 用于释放 recv_res() 的 res
	 */
	void freemsg (EPower::Message *ptr);

	/** 打开后，直接利用 url 构造 GET
	 */
	int send_get ();

	tcpclient *get_tcp_client() { return mp_tc; }

private:
	tcpclient *mp_tc;
	std::string m_url;
};
};

#endif // httpclient.h

