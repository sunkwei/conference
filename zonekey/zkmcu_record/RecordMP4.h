// RecordMP4.h : CRecordMP4 的声明

#pragma once
#include "resource.h"       // 主符号



#include "zkmcu_record_i.h"
#include "_IRecordMP4Events_CP.h"



#if defined(_WIN32_WCE) && !defined(_CE_DCOM) && !defined(_CE_ALLOW_SINGLE_THREADED_OBJECTS_IN_MTA)
#error "Windows CE 平台(如不提供完全 DCOM 支持的 Windows Mobile 平台)上无法正确支持单线程 COM 对象。定义 _CE_ALLOW_SINGLE_THREADED_OBJECTS_IN_MTA 可强制 ATL 支持创建单线程 COM 对象实现并允许使用其单线程 COM 对象实现。rgs 文件中的线程模型已被设置为“Free”，原因是该模型是非 DCOM Windows CE 平台支持的唯一线程模型。"
#endif

using namespace ATL;


// CRecordMP4

class ATL_NO_VTABLE CRecordMP4 :
	public CComObjectRootEx<CComSingleThreadModel>,
	public CComCoClass<CRecordMP4, &CLSID_RecordMP4>,
	public IConnectionPointContainerImpl<CRecordMP4>,
	public CProxy_IRecordMP4Events<CRecordMP4>,
	public IDispatchImpl<IRecordMP4, &IID_IRecordMP4, &LIBID_zkmcu_recordLib, /*wMajor =*/ 1, /*wMinor =*/ 0>
{
public:
	CRecordMP4()
	{
	}

DECLARE_REGISTRY_RESOURCEID(IDR_RECORDMP4)


BEGIN_COM_MAP(CRecordMP4)
	COM_INTERFACE_ENTRY(IRecordMP4)
	COM_INTERFACE_ENTRY(IDispatch)
	COM_INTERFACE_ENTRY(IConnectionPointContainer)
END_COM_MAP()

BEGIN_CONNECTION_POINT_MAP(CRecordMP4)
	CONNECTION_POINT_ENTRY(__uuidof(_IRecordMP4Events))
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



};

OBJECT_ENTRY_AUTO(__uuidof(RecordMP4), CRecordMP4)
