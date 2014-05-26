基于mediastreamer2 的filter，模仿 audio mixer，实现 video mixer。

每录输入，需要指定 '位置', '透明度' 等信息，画到“画布”上，根据设置，压缩画布为 h264 格式，并且通过 output 输出。

支持 20 路 input, 22 路 output，其中 output[20] == 预览输出画布, output[21] == 将输出 h264，可以用于发布到 fms 或者 rtsp server 之类。

支持 zqsender 模式输出，方便兼容 d2000 直播模型