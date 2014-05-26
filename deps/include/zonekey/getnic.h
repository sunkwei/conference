/** 获取网卡信息 */

#ifndef __get_nic__hh
#define __get_nic__hh

#ifdef __cplusplus
extern "C" {
#endif /* c++ */

/** 网卡信息描述，仅仅获取 ethernet, ppp 类型 */
struct zknet_nic_describe
{
	char *name;
	char *desc;
	int type;			// 1: ethernet, 2: ppp，3：wifi （仅仅 win vista 之后，否则wifi被认为 ethernet）
	char *hwaddr;	// 0 结束的硬件地址，一般为 mac 地址
};

/** 获取网卡信息
		@param info 为出参，必须调用 zknet_free_nic_describe() 释放
		@param cnt 出参，返回 zknet_nic_describe 个数
		@return < 0 失败，成功时 = *cnt
 */
int zknet_get_nic_describe(struct zknet_nic_describe **info, int *cnt);
void zknet_free_nic_describe(struct zknet_nic_describe *info, int cnt);

/** 获取ip地址信息

		char **ips = 0;
		int cnt = 0;
		zknet_get_ips(&ips, &cnt);
		for (int i = 0; i < cnt; i++) {
			ips[i] ...
		}
		zknet_free_ips(ips, cnt);
 */
int zknet_get_ips(char ***ips, int *cnt);
void zknet_free_ips(char **ips, int cnt);

#ifdef __cplusplus
}
#endif /* c++ */

#endif /** getnic.h */
