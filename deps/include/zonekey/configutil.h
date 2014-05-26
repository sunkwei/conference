#ifndef _util_config_hh_
#define _util_config_hh_

#ifdef WIN32
#	ifdef _DLL
#		ifdef LIBMYUTIL_EXPORTS
#			define UTIL_CONFIG_API __declspec(dllexport)
#		else
#			define UTIL_CONFIG_API __declspec(dllimport)
#		endif // 
#	else
#		define UTIL_CONFIG_API
#	endif // _DLL
#else
#	define UTIL_CONFIG_API
#endif // 

namespace util 
{
// 需要 Config 的派生类提供的存储格式 

struct ConfigTable
{
	const char *key;
	char type;	// 's'=char*, 'i'=int, 'f'=double
	int offset;	// 存储位置，总使用 this + offset 的地址，保存数据 
	const char *defval;	// 缺省值 
};

/** 为了不再对 non-POD 使用 offsetof(), 使用一个函数完成上面的工作
  	从filename中获取配置, 并且根据 table 写到 obj 中

	@param filename: 配置文件名字
	@param obj: 用户提供的配置数据存储空间, 使用 table 描述结构
	@param table: 配置数据的存储方式描述
	@param tableitems: table 中的条目数
  */
int UTIL_CONFIG_API loadConfig (const char *filename, void *obj, ConfigTable *table, int tableitems);

}; // namespace
#endif // configutil.h

