#include "StdAfx.h"
#include "Zqpkt.h"


Zqpkt::Zqpkt(void)
{
	VariantInit(&data_);
}

Zqpkt::~Zqpkt(void)
{
}

void Zqpkt::set_pkt(zq_pkt *pkt)
{
	// FIXME：这里复制了一遍，更好地做法是使用引用计数，但是这个涉及到修改 zqpsrc，算了
	
	stamp_ = pkt->pts / 45000.0;

	data_.vt = VT_ARRAY | VT_UI1;
	SAFEARRAYBOUND b;
	b.lLbound = 0;
	b.cElements = pkt->len;
	data_.parray = SafeArrayCreate(VT_UI1, 1, &b);
	if (data_.parray) {
		void *ptr;
		if (S_OK == SafeArrayAccessData(data_.parray, &ptr)) {
			memcpy(ptr, pkt->ptr, pkt->len);
			SafeArrayUnaccessData(data_.parray);
		}
	}

	if (pkt->type == 1)
		key_ = pkt->data.video.frametype == 'I';
	else
		key_ = true;	// 音频都是 key
}
