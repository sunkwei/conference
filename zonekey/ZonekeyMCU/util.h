#pragma once

#include <string>
#include <vector>
#include <map>

// 返回自己的 ip 地址
const char *util_get_myip();

typedef std::map<std::string, std::string> KVS;

// 解析诸如 x=0&y=0&width=400&height=300 之类的参数集合，注意，参数不能有重复，否则后面的将覆盖前面的
KVS util_parse_options(const char *options);

// 检查是否所有的 keys 在 kvs 中都存在，如果有不存在的，返回 false
bool chk_params(const KVS &kvs, char info[1024], const char *key, ...);
