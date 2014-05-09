/** 声明 mcu 提供的 webservice 接口
 */

//gsoap mcu service name: mcu
//gsoap mcu service namespace: urn:mcu
//gsoap mcu schema namespace: urn:mcu

typedef struct mcu__Version
{
	int major :1;
	int minor :1;
	char *desc :1;
} mcu__VersionResponse;

// 返回 mcu 版本信息
int mcu__getVersion(void *, mcu__VersionResponse &ver);

// 运行状态
typedef struct mcu__Status
{
    double uptime :1;      // 启动时间，秒
    
    int conferences :1;    // 正在进行的会议数目
    
    double up_kbps5 :1;    // 上传带宽，最后5秒
    double up_kbps60 :1;   //
    double up_kbps300 :1;   //
    
    double down_kbps5 :1;
    double down_kbps60 :1;
    double down_kbps300 :1;
    
    double cpu :1;      // cpu 占用率
    double mem :1;      // 内存 ...
    
    double up_lost_ratio :1;  // 全局丢包率
    double down_lost_ratio :1;
    
} mcu__StatusResponse;

int mcu__getStatus(void *, mcu__StatusResponse &res);

// mediastreamer2 的 media endpoint
typedef struct mcu__MediaEndpoint
{
    char *peerip :1;    // client endpoint
    int peerrtp : 1;
    int peerrtcp : 1;
    
    char *ip : 1;   // mcu 端
    int rtp :1;
    int rtcp :1;
} mcu__MediaEndpoint;

// Source 描述，一个 Source 对应着一个视频点播源
typedef struct mcu__VideoSource
{
    int id  :1;     // 唯一id，大于 0 有效
    int codec :1;   // 编码类型，总是 0，对应 h264

    struct mcu__MediaEndpoint endpoint :1;
} mcu__VideoSourceResponse;

int mcu__addSource(char *memberid, mcu__VideoSourceResponse &res);
int mcu__delSource(char *memberid, char *sourceid, void *);

// Sink 描述，一个 Sink 对应着一个视频点播的接收端
typedef struct mcu__VideoSink
{
    int id :1;      // 唯一 id，大于0有效
    int sourceid :1;    // 点播的 source
  
    struct mcu__MediaEndpoint endpoint :1;
} mcu__VideoSinkResponse;

int mcu__addSink(char *memberid, mcu__VideoSinkResponse &res);
int mcu__delSink(char *memberid, char *sinkid, void *);
