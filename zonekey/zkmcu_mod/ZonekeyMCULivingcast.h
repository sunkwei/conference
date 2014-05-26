// ZonekeyMCULivingcast.h : CZonekeyMCULivingcast 的声明

#pragma once
#include "resource.h"       // 主符号

#include "zkmcu_mod_i.h"
#include "_IZonekeyMCULivingcastEvents_CP.h"
#include <zonekey/rtmp_inte.h>

#if defined(_WIN32_WCE) && !defined(_CE_DCOM) && !defined(_CE_ALLOW_SINGLE_THREADED_OBJECTS_IN_MTA)
#error "Windows CE 平台(如不提供完全 DCOM 支持的 Windows Mobile 平台)上无法正确支持单线程 COM 对象。定义 _CE_ALLOW_SINGLE_THREADED_OBJECTS_IN_MTA 可强制 ATL 支持创建单线程 COM 对象实现并允许使用其单线程 COM 对象实现。rgs 文件中的线程模型已被设置为“Free”，原因是该模型是非 DCOM Windows CE 平台支持的唯一线程模型。"
#endif

using namespace ATL;

// CZonekeyMCULivingcast

class ATL_NO_VTABLE CZonekeyMCULivingcast :
	public CComObjectRootEx<CComSingleThreadModel>,
	public CComCoClass<CZonekeyMCULivingcast, &CLSID_ZonekeyMCULivingcast>,
	public IConnectionPointContainerImpl<CZonekeyMCULivingcast>,
	public CProxy_IZonekeyMCULivingcastEvents<CZonekeyMCULivingcast>,
	public IDispatchImpl<IZonekeyMCULivingcast, &IID_IZonekeyMCULivingcast, &LIBID_zkmcu_modLib, /*wMajor =*/ 1, /*wMinor =*/ 0>
{
	void *rtmp_;
	double audio_begin_, video_begin_;

public:
	CZonekeyMCULivingcast()
	{
		rtmp_ = 0;
	}

DECLARE_REGISTRY_RESOURCEID(IDR_ZONEKEYMCULIVINGCAST)


BEGIN_COM_MAP(CZonekeyMCULivingcast)
	COM_INTERFACE_ENTRY(IZonekeyMCULivingcast)
	COM_INTERFACE_ENTRY(IDispatch)
	COM_INTERFACE_ENTRY(IConnectionPointContainer)
END_COM_MAP()

BEGIN_CONNECTION_POINT_MAP(CZonekeyMCULivingcast)
	CONNECTION_POINT_ENTRY(__uuidof(_IZonekeyMCULivingcastEvents))
END_CONNECTION_POINT_MAP()


	DECLARE_PROTECT_FINAL_CONSTRUCT()

	HRESULT FinalConstruct()
	{
		return S_OK;
	}

	void FinalRelease()
	{
	}

public:
	STDMETHOD(Open)(BSTR url);
	STDMETHOD(Close)(void);
	STDMETHOD(SendH264)(DOUBLE stamp, VARIANT data, BOOL key);
	STDMETHOD(SendAAC)(DOUBLE stamp, VARIANT data);
};

OBJECT_ENTRY_AUTO(__uuidof(ZonekeyMCULivingcast), CZonekeyMCULivingcast)
