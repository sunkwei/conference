#ifndef _httpclient_hh_
#define _httpclient_hh_

/** 一个非常简陋的 http msg parser
 */

#ifdef __cplusplus
extern "C" {
#endif

enum HttpParserState
{
	HTTP_UNKNOWN,
	HTTP_STARTLINE,
	HTTP_HEADERS,
	HTTP_BODY,
	HTTP_COMPLETE,
	HTTP_CR,
	HTTP_CRLF,
	HTTP_LSW,
};

/** key = value */
struct KeyValue
{
	char *key;
	char *value;
};
typedef struct KeyValue KeyValue;

/** url 
		http://192.168.1.103:7789/sample/get?caller=xxx#1

	TODO: 扩展支持 ftp://sunkw:sunkw@192.168.1.107/xxx 格式
*/
struct Url
{
	char *schema;	/** http*/
	char *host;		/** 192.168.1.103:7789 */
	char *path;		/** /sample/get */
	char *query_str; /** caller=xxx */
	char *fragment;	/** 1 */

	KeyValue *query;	/** caller=xxx */
	int query_cnt;		/** 1 */
};
typedef struct Url Url;

/** header */
typedef struct KeyValue HttpHeader;

/** message */
struct HttpMessage
{
	/** start line */
	struct {
		/** GET / HTTP/1.0 */
		/** HTTP/1.0 200 OK */
		char *p1;
		char *p2;
		char *p3;
	} StartLine;

	/** headers */
	HttpHeader *headers;
	int header_cnt;

	/** body */
	char *body;
	int bodylen;

	/** private data */
	void *pri_data;
};
typedef struct HttpMessage HttpMessage;

/** 从字符串, 解析获取 url
 */
Url *httpc_url_create (const char *url);

/** 释放 url 占用资源 */
void httpc_url_release (Url *url);

/** 创建一个空 message */
HttpMessage *httpc_Message_create ();

/** 释放 */
void httpc_Message_release (HttpMessage *msg);

/** 设置message start line */
void httpc_Message_setStartLine (HttpMessage *msg,
		const char *p1, const char *p2, const char *p3);

/** 设置/获取/删除 header */
int httpc_Message_setValue (HttpMessage *msg, const char *key, const char *value);
int httpc_Message_getValue (HttpMessage *msg, const char *key, const char **value);
int httpc_Message_delValue (HttpMessage *msg, const char *key);
int httpc_Message_setValue_printf (HttpMessage *msg, const char *key, const char *vfmt, ...);

/** body */
int httpc_Message_appendBody (HttpMessage *msg, const char *body, int len);
void httpc_Message_clearBody (HttpMessage *msg);
int httpc_Message_getBody (HttpMessage *msg, const char **body);

/** 返回整个message序列化, 需要占用多少字节 */
int httpc_Message_getLength (HttpMessage *msg);
/** 序列化 */
void httpc_Message_encode (HttpMessage *msg, char *buf);

/** 返回解析 message 中间状态 */
enum HttpParserState httpc_Message_state (HttpMessage *msg);
/** 解析消息 */
HttpMessage *httpc_parser_parse (HttpMessage *saved, 
		const char *data, int len, int *used);

#ifdef __cplusplus
}
#endif

#endif /** httpclient.h */
