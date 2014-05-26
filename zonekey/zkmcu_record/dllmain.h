// dllmain.h : 模块类的声明。

class Czkmcu_recordModule : public ATL::CAtlDllModuleT< Czkmcu_recordModule >
{
public :
	DECLARE_LIBID(LIBID_zkmcu_recordLib)
	DECLARE_REGISTRY_APPID_RESOURCEID(IDR_ZKMCU_RECORD, "{241829CA-AD6D-4379-B985-206DF70C6B36}")
};

extern class Czkmcu_recordModule _AtlModule;
