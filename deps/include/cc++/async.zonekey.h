/**
	实现一个类似 proactor 的模型

	class MyRecvfrom : public AsyncOperatorRecvfrom
	{
	     virtual void recvfrom_complete (....)
		 {
			// 
		 }
	};

	AsyncOperationProcessor aop;	// 
	
	AsyncHandler *sock = aop.socket(...);
	MyRecvfrom *readop(sock, 1024);	// 异步操作对象

	aop.async_exec(readop);		// 启动异步操作


	... 当获取数据后，将调用：
			readop->recvfrom_complete(&read, result);
 **/
#ifndef _async_zonekey__hh
#define _async_zonekey__hh

#ifndef CCXX_CONFIG_H_
#include <cc++/config.h>
#endif

#include <cc++/socket.h>

#ifdef CCXX_NAMESPACES
namespace ost {
namespace zonekey {
#endif // namespace

// 异步操作类型
enum AsyncOperationType
{
	ASYNC_READ,
	ASYNC_WRITE,
	ASYNC_RECVFROM,
	ASYNC_SENDTO,
};

/** 描述一个异步句柄 
 */
typedef struct AsyncHandler_t AsyncHandler_t;

/** 从 AsyncHandler_t 获取真实 handler
 */
int __EXPORT async_get_os_handler (AsyncHandler_t *h);

/** 描述一个异步操作
 */
class __EXPORT AsyncOperation
{
	AsyncHandler_t *fd_;

#ifdef WIN32
	OVERLAPPED overlapped_;
#endif //

public:
	AsyncOperation (AsyncHandler_t *fd) : fd_(fd) 
	{
#ifdef WIN32
		overlapped_.hEvent = 0;
		overlapped_.Offset = overlapped_.OffsetHigh = 0;
#endif 
	}
	virtual ~AsyncOperation () {}

	virtual AsyncOperationType type() const = 0;
	virtual void complete(int err_code, int bytes) = 0;

	AsyncHandler_t *fd() const { return fd_; }

#ifdef WIN32
	OVERLAPPED *overlapped() { return &overlapped_; }
#endif // 
};

class __EXPORT AsyncOperation_Read : public AsyncOperation
{
	char *buf_, *outer_;
	int expsize_;

public:
	AsyncOperation_Read (AsyncHandler_t *fd, char *buf, int size)
		: AsyncOperation(fd)
	{
		buf_ = 0;
		outer_ = buf;
		expsize_ = size;		
	}
	AsyncOperation_Read (AsyncHandler_t *fd, int size)
		: AsyncOperation(fd)
	{
		buf_ = new char[size];
		outer_ = 0;
		expsize_ = size;
	}
	~AsyncOperation_Read ()
	{
		delete []buf_;
	}
	
	/** 派生类中重载，err_code = 0, bytes为实际得到的字节数，如果失败，err_code 为错误码 

			@param err_code: 0 成功，否则为错误值
			@param buf: 数据缓冲指针
			@param bytes: buf中有效数据的长度，对于 socket，如果0，说明对方主动关闭连接
	 */
	virtual void read_complete (int err_code, char *buf, int bytes) = 0;

	/** 希望读取的字节数 */
	int expsize() const { return expsize_; }
	char *buf() const { return outer_ ? outer_ : buf_; }

private:
	void complete(int err, int bytes)
	{
		read_complete(err, outer_ ? outer_ : buf_, bytes);
	}
	AsyncOperationType type () const { return ASYNC_READ; }
};

class __EXPORT AsyncOperation_Write : public AsyncOperation
{
	const char *buf_;
	int expsize_;
public:
	AsyncOperation_Write (AsyncHandler_t *fd, const char *buf, int size) : AsyncOperation(fd)
	{
		buf_ = buf;
		expsize_ = size;
	}

	virtual void write_complete (int err_code, const char *buf, int buyes) = 0;

	int expsize() const { return expsize_; }
	const char *buf() const { return buf_; }

protected:
	void complete (int err, int bytes)
	{
		write_complete(err, buf_, bytes);
	}
	AsyncOperationType type() const { return ASYNC_WRITE; }
};

class __EXPORT AsyncOperation_Recvfrom : public AsyncOperation
{
	char *buf_, *outer_;
	int expsize_;
	sockaddr_in from_;
	int fromlen_;

public:
	AsyncOperation_Recvfrom (AsyncHandler_t *h, char *buf, int size)
		: AsyncOperation(h)
	{
		buf_ = 0;
		outer_ = buf;
		expsize_ = size;
	}
	AsyncOperation_Recvfrom (AsyncHandler_t *h, int size)
		: AsyncOperation(h)
	{
		outer_ = 0;
		buf_ = new char[size];
		expsize_ = size;
	}
	~AsyncOperation_Recvfrom ()
	{
		delete []buf_;
	}

	virtual void recvfrom_complete (int err_code, char *buf, int recved, sockaddr *from, int fromlen) = 0;

	char *buf() const { return outer_ ? outer_ : buf_; }
	int expsize() const { return expsize_; }
	sockaddr *from() { return (sockaddr*)&from_; }
	int *fromlenp() 
	{ 
		fromlen_ = sizeof(from_);
		return &fromlen_; 
	}

protected:
	AsyncOperationType type() const { return ASYNC_RECVFROM; }
	void complete (int err_code, int bytes)
	{
		recvfrom_complete(err_code, outer_ ? outer_ : buf_, bytes, (sockaddr*)&from_, fromlen_);
	}
};

class __EXPORT AsyncOperation_Sendto : public AsyncOperation
{
	const char *data_;
	int len_;
	std::string ipv4_;
	int port_;
public:
	AsyncOperation_Sendto (AsyncHandler_t *h, const char *data, int len, const char *ipv4, int port)
		: AsyncOperation(h)
	{
		data_ = data;
		len_ = len;
		ipv4_ = std::string(ipv4);
		port_ = port;
	}

	virtual void send_complete (int err_code) = 0;

	const char *data() const { return data_; }
	int datalen() const { return len_; }
	const char *ipv4() const { return ipv4_.c_str(); }
	int port() const { return port_; }

protected:
	AsyncOperationType type() const { return ASYNC_SENDTO; }
	void complete (int err_code, int bytes)
	{
		send_complete(err_code);
	}
};

/** 对应 proactor 中的 AOP
	windows 下使用完成端口实现
	linux 下使用 epoll 模拟
 */
class __EXPORT AsyncOperationProcessor
{
public:
	AsyncOperationProcessor ();
	~AsyncOperationProcessor ();

	/** 返回一个异步句柄 */
	AsyncHandler_t *socket(int af, int type, int protocol);	// 返回一般的 socket 句柄
	AsyncHandler_t *open_socket_conn (const char *host, const char *service);	// 返回一个 tcp socket，已经连接
	AsyncHandler_t *open_socket_exist (int af);			// 使用已有的 socket构造
	AsyncHandler_t *open_file(const char *filename, const char *mode);

	/** 关闭句柄 */
	void close (AsyncHandler_t *fd);

	/** 启动一个异步执行 */
	int async_exec (AsyncOperation *op);

private:
	void *internal_;
};

#ifdef CCXX_NAMESPACES
}; // zonekey
}; // ost
#endif // namespace

#endif // async_zonekey__hh
