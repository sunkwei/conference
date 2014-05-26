AudioConferenceServer 的目标是作为一个音频会议服务，支持多路语音混合。

AudioConferenceServer 将利用 xmpp (zkrobot_ex）传输控制命令.
                        利用 mediaStreamer2 (from linphone）传输音频rtp流


2012-11-12: 
		新建 Conference 类，封装 mediaStreamer2 中的 audio conference 接口

2012-11-7：本期目标：
              仅仅支持一个全局会议；
			  client 测试使用 mediastream 工具配合 pidgin，首先利用 pidgin 从 AudioConferenceServer 获取
			      会议连接参数，主要包括远端的 ip:port