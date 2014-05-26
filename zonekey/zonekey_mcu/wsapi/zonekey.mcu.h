/** 使用 gsoap 声明 mcu 接口

	重新实现 Server.cpp 中，收到 xmpp 消息的方法 ...
 */

//gsoap zkmcu service name: zkmcu
//gsoap zkmcu service namespace: urn:zkmcu
//gsoap zkmcu schema name: zkmcu

/** get_version
 */
int zkmcu__getVersion(void *, char **info);

/** sys_info
 */
struct zkmcu__SysInfo
{
	double cpu :1;
	double mem :1;
	double net_USCOREsent :1;
	double net_USCORErecv :1;
};
typedef struct zkmcu__SysInfo zkmcu__SysInfoResponse;

int zkmcu__getSysInfo(void *, zkmcu__SysInfoResponse &res);

/** create conference
 */
struct zkmcu__CreateConference
{
	int mode :1;	// 0: free, 1 director
	int livingcast :0;	// 是否支持直播，录像？
	char *desc :0;
};

struct zkmcu__CreateConferenceResponse
{
	int cid :1;
	char *reason :0;
};

int zkmcu__createConference(struct zkmcu__CreateConference *req, 
	struct zkmcu__CreateConferenceResponse &res);

/** destroy conference
 */
int zkmcu__destroyConference(int cid, void *);

/** list_conferences
 */
struct zkmcu__ConferenceIdArray
{
	int *__ptr :1;	// cids array
	int __size :1;
};
typedef struct zkmcu__ConferenceIdArray zkmcu__ConferenceIdArrayResponse;

int zkmcu__listConferences(void *, zkmcu__ConferenceIdArrayResponse &res);

/** info conference，直接将 json 中的信息都释放出来 :)
 */
struct zkmcu__StreamInfo
{
	int streamid :1;
	char *desc :1;

	long sent :1;
	long recv :1;
	long lost_USCOREsent :1;
	long lost_USCORErecv :1;
	int jitter :1;
};

struct zkmcu__StreamInfoArray
{
	struct zkmcu__StreamInfo *__ptr :1;
	int __size :1;
};

struct zkmcu__SourceInfo
{
	int sourceid :1;
	char *desc :1;

	long lost :1;
	long recv :1;
};

struct zkmcu__SourceInfoArray
{
	struct zkmcu__SourceInfo *__ptr :1;
	int __size :1;
};

struct zkmcu__SinkInfo
{
	int sinkid :1;
	char *desc :1;

	long sent :1;
	long lost :1;
	int jitter :1;
};

struct zkmcu__SinkInfoArray
{
	struct zkmcu__SinkInfo *__ptr :1;
	int __size :1;
};

struct zkmcu__ConferenceInfo
{
	int cid :1;
	int mode :1;	// 
	double uptime :1;
	char *desc :1;	

	struct zkmcu__StreamInfoArray *streams :1;
	struct zkmcu__SourceInfoArray *sources :1;
	struct zkmcu__SinkInfoArray *sinks :1;
};
typedef struct zkmcu__ConferenceInfo zkmcu__ConferenceInfoResponse;

int zkmcu__infoConference(int cid, zkmcu__ConferenceInfoResponse &res);

/** add source
 */

/// 对应一个 ortp peer 
struct zkmcu__MediaEndPoint
{
	int rtp_USCOREport :1;
	int rtcp_USCOREport :1;
	char *server_USCOREip :1;
};

struct zkmcu__AddSourceRequest
{
	int payload :1;		// 媒体类型
	char *desc :0;
};

struct zkmcu__AddSourceResponse
{
	int sourceid :1;	// 
	char *reason :0;

	struct zkmcu__MediaEndPoint media :1;
};

int zkmcu__addSource(int cid, struct zkmcu__AddSourceRequest *req, 
	struct zkmcu__AddSourceResponse &res);

/** del source */
int zkmcu__delSource(int cid, int sourceid, void *);

/** add sink
 */
struct zkmcu__AddSinkRequest
{
	int payload :1;	
	char *desc :0;
};

struct zkmcu__AddSinkResponse
{
	int sinkid :1;
	char *reason :0;

	struct zkmcu__MediaEndPoint media :1;
};

int zkmcu__addSink(int cid, struct zkmcu__AddSinkRequest *req,
	struct zkmcu__AddSinkResponse &res);

/** del sink
 */
int zkmcu__delSink(int cid, int sinkid, void *);


/** add stream
 */
struct zkmcu__AddStreamRequest
{
	int payload :1;
	char *desc :0;
};

struct zkmcu__AddStreamResponse
{
	int streamid :1;
	char *reason :0;

	struct zkmcu__MediaEndPoint media :1;
};

int zkmcu__addStream(int cid, struct zkmcu__AddStreamRequest *req,
	struct zkmcu__AddStreamResponse &res);

/** del stream */
int zkmcu__delStream(int cid, int sourceid, void *);

struct zkmcu__KeyValue
{
	char *key :1;
	char *value :1;
};

struct zkmcu__KeyValueArray
{
	struct zkmcu__KeyValue *__ptr :1;
	int __size :1;
};
typedef struct zkmcu__KeyValueArray zkmcu__KeyValueArrayResponse;

int zkmcu__setParams(int cid, struct zkmcu__KeyValueArray *req, void *);
int zkmcu__getParams(int cid, zkmcu__KeyValueArrayResponse &res);
