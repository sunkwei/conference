#!/bin/bash

export xmpp_domain='app.zonekey.com.cn'
while true; do
	CNT=`expr $RANDOM % 200`
	echo $CNT

	while [ $CNT -gt 0 ]; do
		./cs_test_vod.exe normaluser ddkk1212 mse_s_000000000000_mcu_0 test.dc.add_sink sid=-1 &
		let CNT=CNT-1
	done
	
	sleep 70
done
