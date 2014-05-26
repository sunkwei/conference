/** 此接口用于方便配置，运行时环境设置

		模仿 getenv()/putenv() 操作环境变量，但此环境变量设置可以对应配置实例

		此为线程安全库，所有api均互斥！

		
 */

#ifndef _zkconfig__hh
#define _zkconfig__hh

#ifdef __cplusplus
extern "C" {
#endif // c++

typedef struct zkconfig_t zkconfig_t;

/** 从文件创建 zkconfig_t 实例，配置文件为典型的 key=value 格式
 */
zkconfig_t *zkcfg_openfile (const char *filename);

/** 创建一个空的 zkconfig_t 实例
 */
zkconfig_t *zkcfg_open ();

/** 释放实例 */
void zkcfg_close (zkconfig_t *cfg);

/** 获取配置，如不存在，返回 null
 */
const char *zkcfg_get (zkconfig_t *cfg, const char *key);

/** 修改配置，如不存在key，则创建
 */
void zkcfg_set (zkconfig_t *cfg, const char *key, const char *value);

/** 获取配置，如不存在，则尝试通过 getenv() 获取
 */
const char *zkcfg_getenv (zkconfig_t *cfg, const char *key);

/** dump 所有配置到文件 */
int zkcfg_savefile (zkconfig_t *cfg, const char *filename);

#ifdef __cplusplus
}
#endif // c++

#endif // zkconfig.h
