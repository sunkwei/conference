// ImageRGB24.cpp : CImageRGB24 µÄÊµÏÖ

#include "stdafx.h"
#include "ImageRGB24.h"
// CImageRGB24

STDMETHODIMP CImageRGB24::get_Width(LONG* pVal)
{
	*pVal = width_;
	return S_OK;
}


STDMETHODIMP CImageRGB24::get_Height(LONG* pVal)
{
	*pVal = height_;
	return S_OK;
}


STDMETHODIMP CImageRGB24::get_BytesPerLine(LONG* pVal)
{
	*pVal = pic_.linesize[0];
	return S_OK;
}


STDMETHODIMP CImageRGB24::get_Data(VARIANT* pVal)
{
	VARIANT var;
	VariantInit(&var);

	var.vt = VT_ARRAY | VT_UI1;

	SAFEARRAYBOUND b;
	b.lLbound = 0;
	b.cElements = pic_.linesize[0] * height_;

	var.parray = SafeArrayCreate(VT_UI1, 1, &b);
	if (!var.parray) {
		return E_OUTOFMEMORY;
	}

	void *ptr;
	if (SafeArrayAccessData(var.parray, &ptr) == S_OK) {
		unsigned char *p = pic_.data[0];
		unsigned char *q = (unsigned char*)ptr;

		for (int i = 0; i < height_; i++) {
			memcpy(q, p, pic_.linesize[0]);
			p += pic_.linesize[0];
			q += pic_.linesize[0];
		}
		SafeArrayUnaccessData(var.parray);

		*pVal = var;
	}

	return S_OK;
}
