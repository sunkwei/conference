// ImageRGB24.h : CImageRGB24 的声明

#pragma once
#include "resource.h"       // 主符号



#include "zqpsrc_raw_image_ax_i.h"



#if defined(_WIN32_WCE) && !defined(_CE_DCOM) && !defined(_CE_ALLOW_SINGLE_THREADED_OBJECTS_IN_MTA)
#error "Windows CE 平台(如不提供完全 DCOM 支持的 Windows Mobile 平台)上无法正确支持单线程 COM 对象。定义 _CE_ALLOW_SINGLE_THREADED_OBJECTS_IN_MTA 可强制 ATL 支持创建单线程 COM 对象实现并允许使用其单线程 COM 对象实现。rgs 文件中的线程模型已被设置为“Free”，原因是该模型是非 DCOM Windows CE 平台支持的唯一线程模型。"
#endif

using namespace ATL;


// CImageRGB24

class ATL_NO_VTABLE CImageRGB24 :
	public CComObjectRootEx<CComSingleThreadModel>,
	public CComCoClass<CImageRGB24, &CLSID_Zonekey_RawImage_RGB24>,
	public IDispatchImpl<IZonekey_RawImage_RGB24, &IID_IZonekey_RawImage_RGB24, &LIBID_zqpsrc_raw_image_axLib, /*wMajor =*/ 1, /*wMinor =*/ 0>
{
	AVPicture pic_;
	int width_, height_;

public:
	CImageRGB24()
	{
	}

DECLARE_REGISTRY_RESOURCEID(IDR_IMAGERGB24)


BEGIN_COM_MAP(CImageRGB24)
	COM_INTERFACE_ENTRY(IZonekey_RawImage_RGB24)
	COM_INTERFACE_ENTRY(IDispatch)
END_COM_MAP()



	DECLARE_PROTECT_FINAL_CONSTRUCT()

	HRESULT FinalConstruct()
	{
		return S_OK;
	}

	void FinalRelease()
	{
		avpicture_free(&pic_);
	}

public:

	AVPicture &prepare(PixelFormat pf, int width, int height)
	{
		width_ = width, height_ = height;
		avpicture_alloc(&pic_, pf, width, height);
		return pic_;
	}
	STDMETHOD(get_Width)(LONG* pVal);
	STDMETHOD(get_Height)(LONG* pVal);
	STDMETHOD(get_BytesPerLine)(LONG* pVal);
	STDMETHOD(get_Data)(VARIANT* pVal);
};

OBJECT_ENTRY_AUTO(__uuidof(Zonekey_RawImage_RGB24), CImageRGB24)
