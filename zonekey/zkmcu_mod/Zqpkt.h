#pragma once

#include <zonekey/zqpsource.h>

class Zqpkt
{
	double stamp_;
	VARIANT data_;
	bool key_;

public:
	Zqpkt(void);
	~Zqpkt(void);

	void set_pkt(zq_pkt *pkt);

	VARIANT get_data() const { return data_; }	// 返回使用 VARIANT 封装的 array
	DOUBLE get_stamp() const { return stamp_; }	// 返回 秒 单位的时间戳
	bool is_key_frame() const { return key_; }
};
