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

