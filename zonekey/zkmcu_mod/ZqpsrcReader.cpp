// ZqpsrcReader.cpp : CZqpsrcReader 的实现

#include "stdafx.h"
#include "ZqpsrcReader.h"
#include <assert.h>

// CZqpsrcReader

STDMETHODIMP CZqpsrcReader::Open(BSTR url)
{
	_bstr_t Url(url);
	zqpsrc_open(&src_, Url);

	return S_OK;
}


STDMETHODIMP CZqpsrcReader::Close(void)
{
	if (src_) {
		zqpsrc_close(src_);
		src_ = 0;
	}

	return S_OK;
}


STDMETHODIMP CZqpsrcReader::GetNextPacket(IDispatch** package)
{
	if (!src_) *package = 0;
	else {
		zq_pkt *pkt = zqpsrc_getpkt(src_);
		if (!pkt) {
			zqpsrc_close(src_);
			src_ = 0;
			*package = 0;
		}
		else {
			// 根据类型，构造 ...
			if (pkt->type == 1) {
				// video
				IPackageForVideo *iv;
				if (CPackageForVideo::CreateInstance(&iv) == S_OK) {
					CPackageForVideo *pv = (CPackageForVideo*)iv;
					pv->set_pkt(pkt);

					*package = iv;
				}
				else {
					assert(false);
				}
			}
			else {
				// audio
				IPackageForAudio *ia;
				if (CPackageForAudio::CreateInstance(&ia) == S_OK) {
					CPackageForAudio *pa = (CPackageForAudio*)ia;
					pa->set_pkt(pkt);

					*package = ia;
				}
				else {
					assert(false);
				}
			}

			zqpsrc_freepkt(src_, pkt);
		}
	}
	return S_OK;
}

