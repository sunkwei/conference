#pragma once

/**

	2013-8-20: v0.4.8.15 sunkw
		DirectorConference: 根据 create_conference() 参数 livingcast=[true|false] 决定是否启动 video mixer


	2013-8-19: v0.4.8.13 sunkw
		info_conference() 提供 conference 持续时间，方便计算码率....

	2013-8-8: v0.4.8: sunkw
		提供详细的 rtp stats 和 rtcp stats 信息

	2013-8-6: v0.4.6: sunkw:
		扩展 info_conference 方法，返回 JSON 格式的详细描述，但是看起来统计信息不准确啊 ....

	2013-7-3: v0.4.4: sunkw:
		新增方法 dc.get_all_videos 返回指定 cid 的所有视频 source/stream 及其描述
		新增方法，交换两个 video stream 的位置

	2013-6-18: v0.4.1: sunkw:
		DirectorConference 的 audio publisher 使用 iLBC 压缩，输出

	2013-6-7: v0.4.x: sunkw：
		新增支持：addStreamWithPublisher() 该 stream 参与合成，并且 Stream->get_publisher_filter() 能够返回一个 publisher 对象，以支持点播

	2013-4-4: v0.3.4: sunkw:
		新增 add_stream( payload type = 0 (PCM uLaw)），用于支持来自 gips 的 client
 */

#define VERSION_STR ("v0.4.8.18: build at: " __TIMESTAMP__)
