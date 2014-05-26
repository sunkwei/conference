rem 设置 xmpp_domain 环境变量，根据自己的环境设置
set xmpp_domain=192.168.12.201

rem 启动来自 jp100hd 的数据源，url 换成 d2000 直播源，或者 jp100hd 的直播
call test_zonekeyh264source.exe  tcp://172.16.1.12:3011
