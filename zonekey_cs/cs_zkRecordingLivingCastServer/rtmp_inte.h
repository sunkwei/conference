#ifndef TRMP_INTE_H
#define TRMP_INTE_H
/*@file
********************************************************************************
模块名 :rtmplive
文件名 : rtmp_inte.h
相关文件 :rtmp_live_publisher.h
文件实现功能 :提供接口供C#调用
作者 : LSQ
2013/05/08
*******************************************************************************/
	
#ifdef __cplusplus
#define EXPORT extern "C" __declspec (dllexport)
#else
#define EXPORT __declspec (dllexport)
#endif


/*! @function
********************************************************************************
函数名 : Init_Rtmp
功能 : 初始化对象
参数 : (void** rtmp)
		参数1：创建对象，申请内存
返回值 : 状态值（0：失败，1：成功）
</PRE>
*******************************************************************************/
EXPORT int Init_Rtmp(void** rtmp);

/*! @function
********************************************************************************
函数名 : UnInit_Rtmp
功能 : 释放内存
参数 : (void* rtmp)
		参数1：操作对象
返回值 : 无返回
</PRE>
*******************************************************************************/
EXPORT void UnInit_Rtmp(void* rtmp);

/*! @function
********************************************************************************
函数名 : Open_Rtmp
功能 : 打开并建立连接
参数 : (void* rtmp， const char url[])
		参数1：操作对象
		参数2：建立连接的url，url格式为rtmp://fmsIp/livepkgr/{任意一个名称}?adbe-live-event=liveevent
返回值 : 状态值（0：失败，1：成功）
</PRE>
*******************************************************************************/
EXPORT int Open_Rtmp(void* rtmp, const char url[]);

/*! @function
********************************************************************************
函数名 : Close_Rtmp
功能 : 断开连列
参数 : (void* rtmp)
		参数1：操作对象
返回值 : 无返回
</PRE>
*******************************************************************************/
EXPORT void Close_Rtmp(void* rtmp);

/*! @function
********************************************************************************
函数名 : SendVideoPacket_Rtmp
功能 : 发送视频信息
参数 : (void* rtmp, const void *data, int len, int key,double stamp)
		参数1：操作对象
		参数2：视频字符串
		参数3：字符串长度
		参数4：关键帧
		参数5：时间戳(单位：毫秒)
返回值 : 状态值（0：失败，1：成功）
</PRE>
*******************************************************************************/
EXPORT int SendVideoPacket_Rtmp(void* rtmp, const void *data, int len, int key, double stamp);

/*! @function
********************************************************************************
函数名 : SendMedaDatePacket_Rtmp
功能 : 发送媒体信息
参数 : (void* rtmp, void* meidainfo)
		参数1：操作对象
		参数2：视频宽度
		参数3：视频高度
		参数4：视频帧率
		参数5：音频bit值（默认值：16）
		参数6：音频采样率（默认值：32000）
		参数7：音频管道（默认值：2）
返回值 : 状态值（1：成功，其它：失败）
</PRE>
*******************************************************************************/
EXPORT int SendMedaDatePacket_Rtmp(void* rtmp, const int videowidth, const int videoheight, const int fps, 
								   const int samplesize, const int samplerate, const int audio_chs);


/*! @function
********************************************************************************
函数名 : UnInit_Rtmp
功能 : 发送音频信息
参数 : (void* rtmp, const void *data, int len ,double stamp)
		参数1：操作对象
		参数2：音频字符串
		参数3：字符串长度
		参数4：时间戳（单位：毫秒）
返回值 : 状态值（0：失败，1：成功）
</PRE>
*******************************************************************************/
EXPORT int SendAudioPacket_Rtmp(void* rtmp, const void *data, int len ,double stamp);

#endif