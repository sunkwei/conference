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
    int sourceid  :1;     // 全局唯一id，大于 0 有效
    int codec :1;   // 编码类型，总是 0，对应 h264

    struct mcu__MediaEndpoint endpoint :1;
} mcu__VideoSourceResponse;

struct mcu__VideoSourceArray
{
    struct mcu__VideoSource *__ptr :1;
    int __size :1;
};

int mcu__addVideoSource(int confid, int memberid, mcu__VideoSourceResponse &res); // 根据 res.sourceid 判断是否成功
int mcu__delVideoSource(int confid, int memberid, int sourceid, void *);

// Sink 描述，一个 Sink 对应着一个视频点播的接收端
typedef struct mcu__VideoSink
{
    int sinkid :1;      // 全局唯一 id，大于0有效
    int sourceid :1;    // 点播的 source
  
    struct mcu__MediaEndpoint endpoint :1;
} mcu__VideoSinkResponse;

struct mcu__VideoSinkArray
{
    struct mcu__VideoSink *__ptr;
    int __size :1;
};

int mcu__addVideoSink(int confid, int memberid, int sourceid, mcu__VideoSinkResponse &res);
int mcu__delVideoSink(int confid, int memberid, int sinkid, void *);

// Stream, 一个 stream 为双向 rtp，目前仅仅用于音频 ...
typedef struct mcu__AudioStream
{
    int streamid :1;    // 全局唯一id，大于0有效
    
    struct mcu__MediaEndpoint endpoint :1;
    
} mcu__AudioStreamResponse;

int mcu__addAudioStream(int confid, int memberid, mcu__AudioStreamResponse &res);
int mcu__delAudioStream(int confid, int memberid, int streamid, void *);

// Member ，描述一个成员，包含数个 Source 和 Sink
typedef struct mcu__Member
{
    int memberid :1;      // 唯一id，大于 0 有效
    char *name :1;  // 显示名字？？
    
    struct mcu__VideoSourceArray sources :1;
    struct mcu__VideoSinkArray sinks :1;
    struct mcu__AudioStream audio :1;
    
    int enableaudio :1;         // 是否启用音频 ...
} mcu__MemberResponse;

struct mcu__MemberArray
{
    struct mcu__Member *__ptr :1;
    int __size :1;
};

int mcu__addMember(int confid, mcu__MemberResponse &res);   // 通过 res.memberid 判断是否成功
int mcu__delMember(int confid, int memberid, void *);
int mcu__enableSound(int confid, int memberid, int enable, int &res);   // res 是否成功

// Conference, 描述一个会议，由一堆 Member 构成
typedef struct mcu__Conference
{
    int confid : 1;         // 会议唯一标识，大于0有效
    char *desc : 1;         // 会议描述？？
    
    struct mcu__MemberArray members :1; // 会议成员
} mcu__ConferenceResponse;

typedef struct mcu__ConferenceArray
{
    struct mcu__Conference *__ptr :1;
    int __size :1;
} mcu__ConferenceArrayResponse;

int mcu__addConference(void *, mcu__ConferenceResponse &res);   // 通过 res.confid 判断是否成功
int mcu__delConference(int confid, void *);			// 删除 confid 对应的会议
int mcu__listConferences(void *, mcu__ConferenceArrayResponse &res);    // 返回所有会议 ..
