// dllmain.cpp : DllMain 的实现。

#include "stdafx.h"
#include "resource.h"
#include "zkmcu_record_i.h"
#include "dllmain.h"
#include "xdlldata.h"

Czkmcu_recordModule _AtlModule;

// DLL 入口点
extern "C" BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
#ifdef _MERGE_PROXYSTUB
	if (!PrxDllMain(hInstance, dwReason, lpReserved))
		return FALSE;
#endif
	hInstance;
	return _AtlModule.DllMain(dwReason, lpReserved); 
}
