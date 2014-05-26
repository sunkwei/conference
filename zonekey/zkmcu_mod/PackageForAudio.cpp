// PackageForAudio.cpp : CPackageForAudio 的实现

#include "stdafx.h"
#include "PackageForAudio.h"


// CPackageForAudio

STDMETHODIMP CPackageForAudio::get_Data(VARIANT* pVal)
{
	// TODO: 在此添加实现代码
	*pVal = get_data();
	return S_OK;
}


STDMETHODIMP CPackageForAudio::get_stamp(DOUBLE* pVal)
{
	// TODO: 在此添加实现代码
	*pVal = Zqpkt::get_stamp();
	return S_OK;
}


STDMETHODIMP CPackageForAudio::DataType(LONG* type)
{
	// TODO: 在此添加实现代码
	*type = 2;
	return S_OK;
}
