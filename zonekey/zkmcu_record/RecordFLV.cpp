// RecordFLV.cpp : CRecordFLV 的实现

#include "stdafx.h"
#include "RecordFLV.h"

// CRecordFLV
STDMETHODIMP CRecordFLV::Open(BSTR filename, LONG av)
{
	_bstr_t Filename(filename);
	flv_ = flv_writer_open(Filename);

	return S_OK;
}

STDMETHODIMP CRecordFLV::Stop(void)
{
	if (flv_) {
		flv_writer_close(flv_);
		flv_ = 0;
	}

	return S_OK;
}

STDMETHODIMP CRecordFLV::SaveH264Frame(DOUBLE stamp, BOOL key_frame, VARIANT data)
{
	HRESULT hr = E_FAIL;

	if (flv_ && data.vt == (VT_ARRAY | VT_UI1)) {
		if (SafeArrayGetDim(data.parray) != 1)
			return E_INVALIDARG;

		LONG lb, ub;
		hr = SafeArrayGetLBound(data.parray, 1, &lb);
		if (hr != S_OK) return S_OK;
		hr = SafeArrayGetUBound(data.parray, 1, &ub);
		if (hr != S_OK) return S_OK;

		void *ptr;
		hr = SafeArrayAccessData(data.parray, &ptr);
		if (hr == S_OK) {
			int len = ub - lb + 1;
			flv_writer_save_h264(flv_, (unsigned char*)ptr + lb, len, key_frame, stamp, 0, 0);	// FIXME: width, height 没有使用
			SafeArrayUnaccessData(data.parray);
			return hr;
		}
		else
			return S_OK;
	}

	return hr;
}

STDMETHODIMP CRecordFLV::SaveAACFrane(DOUBLE stamp, BOOL adts_head, VARIANT data)
{
	HRESULT hr = E_FAIL;

	if (flv_ && data.vt == (VT_ARRAY | VT_UI1)) {
		if (SafeArrayGetDim(data.parray) != 1)
			return E_INVALIDARG;

		LONG lb, ub;
		hr = SafeArrayGetLBound(data.parray, 1, &lb);
		if (hr != S_OK) return hr;
		hr = SafeArrayGetUBound(data.parray, 1, &ub);
		if (hr != S_OK) return hr;

		void *ptr;
		hr = SafeArrayAccessData(data.parray, &ptr);
		if (hr == S_OK) {
			int len = ub - lb + 1;
			flv_writer_save_aac(flv_, (unsigned char*)ptr + lb, len, stamp);
			SafeArrayUnaccessData(data.parray);
			return hr;
		}
	}
	else {
		return hr;
	}
}
