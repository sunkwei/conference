// RecordFLV.h : CRecordFLV 的声明

#pragma once
#include "resource.h"       // 主符号

#include "zkmcu_record_i.h"
#include "_IRecordFLVEvents_CP.h"
#include "flv_writer.h"

#if defined(_WIN32_WCE) && !defined(_CE_DCOM) && !defined(_CE_ALLOW_SINGLE_THREADED_OBJECTS_IN_MTA)
#error "Windows CE 平台(如不提供完全 DCOM 支持的 Windows Mobile 平台)上无法正确支持单线程 COM 对象。定义 _CE_ALLOW_SINGLE_THREADED_OBJECTS_IN_MTA 可强制 ATL 支持创建单线程 COM 对象实现并允许使用其单线程 COM 对象实现。rgs 文件中的线程模型已被设置为“Free”，原因是该模型是非 DCOM Windows CE 平台支持的唯一线程模型。"
#endif

using namespace ATL;

// CRecordFLV

class ATL_NO_VTABLE CRecordFLV :
	public CComObjectRootEx<CComSingleThreadModel>,
	public CComCoClass<CRecordFLV, &CLSID_RecordFLV>,
	public IConnectionPointContainerImpl<CRecordFLV>,
	public CProxy_IRecordFLVEvents<CRecordFLV>,
	public IDispatchImpl<IRecordFLV, &IID_IRecordFLV, &LIBID_zkmcu_recordLib, /*wMajor =*/ 1, /*wMinor =*/ 0>
{
	flv_writer_t *flv_;

public:
	CRecordFLV()
	{
		flv_ = 0;
	}

DECLARE_REGISTRY_RESOURCEID(IDR_RECORDFLV)


BEGIN_COM_MAP(CRecordFLV)
	COM_INTERFACE_ENTRY(IRecordFLV)
	COM_INTERFACE_ENTRY(IDispatch)
	COM_INTERFACE_ENTRY(IConnectionPointContainer)
END_COM_MAP()

BEGIN_CONNECTION_POINT_MAP(CRecordFLV)
	CONNECTION_POINT_ENTRY(__uuidof(_IRecordFLVEvents))
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



	STDMETHOD(Open)(BSTR filename, LONG av);
	STDMETHOD(Stop)(void);
	STDMETHOD(SaveH264Frame)(DOUBLE stamp, BOOL key_frame, VARIANT data);
	STDMETHOD(SaveAACFrane)(DOUBLE stamp, BOOL adts_head, VARIANT data);
};

OBJECT_ENTRY_AUTO(__uuidof(RecordFLV), CRecordFLV)
