// dllmain.h : 模块类的声明。

class Czkmcu_modModule : public ATL::CAtlDllModuleT< Czkmcu_modModule >
{
public :
	DECLARE_LIBID(LIBID_zkmcu_modLib)
	DECLARE_REGISTRY_APPID_RESOURCEID(IDR_ZKMCU_MOD, "{5706B58C-FE27-425F-A87F-BB328FF92B10}")
};

extern class Czkmcu_modModule _AtlModule;
