/** zkrtp 用于实现rtp打包/解包传输媒体数据

  	@date 2012-3-14: 开始增加 rtcp 的完整支持
 */

#ifndef _zkrtp_interface_hh
#define _zkrtp_interface_hh

#ifdef __cplusplus
extern "C"{
#endif

/** rtp session 实例 */
typedef struct zkrtp_session_t  zkrtp_session_t;
struct zkrtp_cfg;

/** 为了支持多种打包实现 */
struct zkrtp_packer_funcs 
{
	zkrtp_session_t *(*open)(zkrtp_cfg *cfg);	// 对应 zkrtp_open_session
	void (*close)(zkrtp_session_t *s);			// 对应 zkrtp_close_session
	void (*send_media)(zkrtp_session_t *s, const void *data, int len,double stamp, int key);	// 对应 zkrtp_send_media()
	void (*recv_media)(zkrtp_session_t *s, const void *data, int len);
};

/** 目前 rtp 支持的媒体类型 */
enum ZKRTP_MEDIA_TYPE
{
	// 音频类型
	ZKRTP_MEDIA_AUDIO_SPEEX = 1,
	ZKRTP_MEDIA_AUDIO_AAC,

	// 视频类型
	ZKRTP_MEDIA_VIDEO_H264 = 100,

	// 字幕
	ZKRTP_MEDIA_SUBTITLE = 1000,
};

/** RTCP SR info
 */
typedef struct zkrtp_rtcp_SenderInfo
{
	unsigned int ssrc;	/** 发送者 ssrc */
	unsigned int rtp_stamp;	/** 发送者最新报的rtp时间戳，使用rtp payload timer */
	unsigned int ntp, ntp_frac;	/** 发送者 NTP 时间，ntp 为秒，ntp_frac 为小数部分 */
	unsigned int cnt_pkgs, cnt_bytes;	/** 发送者统计的发送报数，字节数 */
} zkrtp_rtcp_sr;

/** RTCP receiver Report
 */
typedef struct zkrtp_rtcp_Report
{
	unsigned int ssrc;	/* 接收者 ssrc */
	float d_lost;	/** 丢包率，从上次收到 Report 开始 */
	unsigned int sum_lost;	/** 累计丢包率 */
	unsigned int max_seq;	/** 报序号，使用 32bits，不会出现溢出 */
	unsigned int jitter; /** 抖动 */
	unsigned int lsr, dlsr;	/** ??? */
} zkrtp_rtcp_Report;

/** rtp session 配置接口
 */
struct zkrtp_cfg
{
	ZKRTP_MEDIA_TYPE media_type;	// payload type, to audio/video/subtitle ...

	double max_delay;		// 最大缓冲延迟，用于接收缓冲，支持重排序，秒

	unsigned short mtu;   // rtp分包大小
	unsigned short pt;    //负载类型，对应 rtp payload type 字段
	int        time_inc;  //默认时间戳增量 
	int freq;	/** 
			  对应时间戳频率, 如 h264 使用 90000, pcm 使用 8000 之类的
			 */

	union {
		struct {
			int ch, bits, rate;
			int framesize;
			// ... more
		} audio;
		struct {
			int widith, height;
			// ... more
		} video;
		struct {
			int codepage; 
			// ... more
		} subtitle;
	} media;

	// user ptr
	void *userptr;

	// 调用者实现，rtp lib 使用其发送媒体数据
	int (*send_media)(void *userptr, const void *data, int len);

	// 调用者实现，rtp lib 使用其发送rtcp数据
	int (*send_rtcp)(void *userptr, const void *data, int len);

	/** 调用者实现，rtp lib 注册媒体数据回调函数，当收到rtp/rtcp通道数据时，将回调rtp lib 
		提供的 cb 指针

		@param userptr: 对应着 zkrtp_cfg::userptr
		@param cb: 回调函数
		@param opaque: rtp lib 实现者自己的上下文
	 */
	void (*reg_recv_media)(void *userptr, 
		void (*cb)(void *opaque, const void *data, int len),
		void *opaque); // to reg a callback
	void (*unreg_recv_media)(void *userptr, 
		void (*cb)(void *opaque, const void *data, int len)); /** 注销 */

	void (*reg_recv_rtcp)(void *userptr, 
		void (*cb)(void *opaque, const void *data, int len),
		void *opaque); // to reg a callback
	void (*unreg_recv_rtcp)(void *userptr, 
		void (*cb)(void *opaque, const void *data, int len)); /** 注销 */

	/** 当rtplib组成完整数据帧时，回调。
		一般的: rtplib 在其实现的 void (*cb)(void*, const void*, int) 中实现组包，当得到完整
		帧时，调用本回调，通知应用层。因此zkrtp库理论上，应该是应用层驱动的，不应该使用线程！

			@param userptr: zkrtp_cfg::userpt
			@param cfg: 本结构
			@param frame: 帧数据
			@param len: 数据长度
			@param stamp: 时间戳，必须转换为秒
			@param frame_seq: 帧序号，此序号对应“帧”而不是rtp的序号！
			@param frame_safed: 帧是否完整

		一般来说，音频包总是小于mtu的，所以 frame_safed 总是1，而视频帧可能出现中间丢包的情
			况，此时必须设置 frame_safed = 0
	 */
	void (*on_frame)(void *userptr, struct zkrtp_cfg *cfg, 
		const void *frame, int len, double stamp, unsigned short frame_seq, int frame_safed);

	/** 当 rtplib 收到 rtcp SR 包时，回调，可选 （可选的意思是，如果app不关心，这个函数指针应该设置为 0）
			@param userptr:
			@param si: 发送者状态描述
			@param rc: report 数目
			@param reports: report 数组
	 */
	void (*on_rtcp_sr)(void *userptr, const zkrtp_rtcp_SenderInfo *si,
		int rc, const zkrtp_rtcp_Report *reports);


	/** 当 rtplib 收到 rtcp RR 报时调用，可选
	 */
	void (*on_rtcp_rr)(void *userptr, int rc, const zkrtp_rtcp_Report *reports);

	/** 当 rtplib 收到 rtcp SDES 时调用，可选

			@param userptr:
			@param ssrc: 对应 sdes 字段的发送者
			@param type: sdes 类型：包含
								1: CNAME
								2: NAME
								3: EMAIL
								4: PHONE
								5: LOCATION
								6: TOOL
								7: NOTE
								8: USER DEF PRIVATE
			@param len: data 的字节长度
			@param data: 数据，注意，非null结束，需要根据 len 决定
	 */
	void (*on_rtcp_sdes)(void *userptr, unsigned int ssrc, unsigned char type, unsigned char len, const char *data);

	/** 当 rtplib 收到 rtcp BYE 时调用， 可选
	 */
	void (*on_rtcp_bye)(void *userptr, unsigned int ssrc);

	/** 当 rtplib 收到 rtcp APP 时调用，可选
	 */
	void (*on_rtcp_app)(void *userptr, unsigned int ssrc, const char *data, int len);
};
typedef struct zkrtp_cfg zkrtp_cfg;


/** 创建zkrtp session 实例

		@param cfg: 有效的配置信息
		@return 0 失败
				否则为 zkrtp session 实例

	一般实现流程为：
		0. 检查 cfg 属性是否支持
		1. 根据 cfg 指定的属性，创建私有上下文；
		2. 初始化所有相关属性
		3. 调用 cfg->reg_recv_media(), cfg->reg_recv_rtcp()，注册数据通知；
		4. 返回 zkrtp_session_t 指针
 */
zkrtp_session_t *zkrtp_open_session(zkrtp_cfg *cfg);

/** 释放 zkrtp session 实例所有资源

	一般实现流程为：
		0. 转换 sess 为私有上下文；
		1. 调用 cfg->unreg_recv_media(), cfg->unreg_recv_rtcp() 注销数据通知；
		2. 释放所有分配的资源
 */
void zkrtp_close_session(zkrtp_session_t *sess);


/** 发送多媒体数据
	@param pzk[in]: 
	@param data[in]; 要发送的数据
	@param len[in]; data的字节数
	@param stamp: 时间戳，必须转换为秒
	@param key: 是否为关键帧
				当调用 cfg->send_media() 时，可能会返回错误(<0)，当 key 有效时，
				需要
						while (cfg->send_media(...) < 0)
							sleep 1ms
				以确保关键帧发出。

	一般实现流程为：
		0. 转换 sess 为私有上下文；
		1. rtp 打包（分片之类的...）
		2. 对于每个rtp包，调用 cfg->send_media() 发送
 */
void zkrtp_send_media (zkrtp_session_t *sess, const void *data, int len, double stamp, int key);

/** 外部提供的rtcp发送驱动, 需要app主动调用该函数, 此函数不会阻塞, 因此要求app自己 sleep,
    建议至少每隔 1 秒调用一次本函数

    	内部处理:
		将累计 run 调用的间隔时间, 计算是否需要发送 rtcp 报
 */
void zkrtp_rtcp_run (zkrtp_session_t *sess);

#ifdef __cplusplus
};
#endif

#endif
