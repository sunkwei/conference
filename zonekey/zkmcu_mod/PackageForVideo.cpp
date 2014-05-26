// PackageForVideo.cpp : CPackageForVideo 的实现

#include "stdafx.h"
#include "PackageForVideo.h"


// CPackageForVideo



STDMETHODIMP CPackageForVideo::get_Data(VARIANT* pVal)
{
	*pVal = get_data();
	return S_OK;
}


STDMETHODIMP CPackageForVideo::get_Stamp(DOUBLE* pVal)
{
	*pVal = get_stamp();
	return S_OK;
}


STDMETHODIMP CPackageForVideo::get_Key(VARIANT_BOOL* pVal)
{
	// TODO: 在此添加实现代码
	*pVal = is_key_frame() ? -1 : 0;
	return S_OK;
}


STDMETHODIMP CPackageForVideo::DataType(LONG* t)
{
	// TODO: 在此添加实现代码
	*t = 1;
	return S_OK;
}
