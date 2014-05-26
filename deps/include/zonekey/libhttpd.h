#ifndef _media_httpd_hh__
#define _media_httpd_hh__

#include <stdio.h>
#include <string.h>
#include <sstream>
#include "ehttp.h"

#ifdef _LIB
# define HTTPAPI
#else
#ifdef WIN32
#	ifdef HTTPSRV_EXPORTS
#		define HTTPAPI __declspec(dllexport)
#	else
#		define HTTPAPI __declspec(dllimport)
#	endif // HTTPSRV_EXPORTS 
#else
#	define HTTPAPI
#endif // win32
#endif // _LIB

#define _VERSION "wenxi, 1.0.rc1"

namespace EPower {

	class HTTPAPI httpd
	{
	public:
		httpd ();
		virtual ~httpd ();

		int open (int port, const char *bindip);
		int close ();
		int get_thread_info (int *total, int *idle);

	protected:
		/** 一般扩展类需要重新实现这个方法，注意需要检查 req->isComplete()，
		    因为可能body不完整
		 */
		virtual bool onRequest (const void *opaque, Message *req, Message *res)
		{
			if (req->state() == EPower::Message::BODY) {
				size_t rest = req->residualBodyLength();
			}

			res->setValue("Connection", "Close");
			sendres(opaque, res);

			return false;
		}

		int send (const void *opaque, const void *data, size_t len);
		int recv (const void *opaque, void *databuf, size_t len);
		int recvt (const void *opaque, void *databuf, size_t len, int timeout);

		int from (const void *opaque, char ipfrom[128], int *port);

	public:
		int sendres (const void *opaque, const Message *res)
		{
			std::stringstream ss;
			res->encode(ss);
			std::string x = ss.str();

			return send(opaque, x.data(), x.size());
		}

		/** FIXME: 应该使用更好的接口封装返回的 socket */
		int getsock (const void *opaque);

	private:
		void *mp_internal;
		static void tcpconn_cb (void *opaque, SOCKET sock, int port, const char *ip);
		void newConnection (int sock, int port, const char *ip);
		char m_from_ip[128];
		int m_from_port;
	};

}; // namespace EPower

#endif // libhttpd.h

