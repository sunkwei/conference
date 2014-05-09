//
//  KVConfig.h
//  mcu
//
//  Created by sunkw on 14-5-9.
//  Copyright (c) 2014年 sunkw. All rights reserved.
//
//  key value 存储配置...
//  save() 时，保存为 filename.session，每次 load 时，总是首先 load(filename), 再 load(filename.session)

#ifndef __mcu__KVConfig__
#define __mcu__KVConfig__

#include <iostream>
#include <string>
#include <map>
#include <cc++/thread.h>

class KVConfig
{
    std::string filename_;
    typedef std::map<std::string, std::string> KVS;
    ost::Mutex cs_;
    KVS kvs_;
    
public:
    KVConfig(const char *filename);
    ~KVConfig();
    
    const char *get(const char *key, const char *def);
    void set(const char *key, const char *value);
    void set(const char *key, int value);
    void set(const char *key, double value);
    void del(const char *key);
    
    void reload();
    void save();
    void save_as(const char *filename);
    
private:
    void load_from_file(const char *filename);
    void save_to(const char *filename);
};

#endif /* defined(__mcu__KVConfig__) */
