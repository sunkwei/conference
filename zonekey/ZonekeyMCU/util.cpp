#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <WinSock2.h>
#include "util.h"
#include <assert.h>
#include <mediastreamer2/mediastream.h>

const char *util_get_myip()
{
	static std::string _ip;
	if (_ip.empty()) {
		char hostname[512];
		gethostname(hostname, sizeof(hostname));

		struct hostent *host = gethostbyname(hostname);
		if (host) {
			if (host->h_addrtype == AF_INET) {
				in_addr addr;
				
				for (int i = 0; host->h_addr_list[i]; i++) {
					addr.s_addr = *(ULONG*)host->h_addr_list[i];
					_ip = inet_ntoa(addr);
					if (strstr(_ip.c_str(), "192.168.56."))	// virtual box 的ip地址，需要跳过
						continue;
				}
			}
		}
	}

	return _ip.c_str();
}

std::map<std::string, std::string> util_parse_options(const char *options)
{
	std::map<std::string, std::string> kvs;
	std::vector<char *> strs;

	// 使用 strtok() 分割
	char *tmp = strdup(options);
	char *p = strtok(tmp, "&");
	while (p) {
		strs.push_back(p);
		p = strtok(0, "&");
	}

	for (std::vector<char*>::iterator it = strs.begin(); it != strs.end(); ++it) {
		char *key = strtok(*it, "=");
		assert(key);

		char *value = strtok(0, "\1");	// FIXME:
		if (!value)
			value = "";

		kvs[key] = value;
	}

	free(tmp);
	return kvs;
}

bool chk_params(const KVS &kvs, char info[1024], const char *key, ...)
{
	bool ok = true;
	va_list list;
	va_start(list, key);
	strcpy(info, "");

	while (key) {
		KVS::const_iterator itf = kvs.find(key);
		if (itf == kvs.end()) {
			ok = false;
			snprintf(info, 1024, "'%s' not found", key);
			break;
		}

		key = va_arg(list, const char*);
	}
	va_end(list);

	return ok;	// 都存在
}
