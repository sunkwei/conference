基于 mediastreamer2 创建一个测试程序，用于显示收到的 h264

filter链路为： rtp recv --> h264 decoder --> zonekey.yuv.sink
将 zonekey.yuv.sink 收到的yuv图像，通过 render-video 显示出来。
