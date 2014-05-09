//
//  KVConfig.cpp
//  mcu
//
//  Created by sunkw on 14-5-9.
//  Copyright (c) 2014年 sunkw. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include "KVConfig.h"
#include <sys/stat.h>

KVConfig::KVConfig(const char *fname) : filename_(fname)
{
    reload();
}

KVConfig::~KVConfig()
{
    
}

static char *trim(char *str)
{
    char *end;
    
    // Trim leading space
    while(isspace(*str)) str++;
    
    if(*str == 0)  // All spaces?
        return str;
    
    // Trim trailing space
    end = str + strlen(str) - 1;
    while(end > str && isspace(*end)) end--;
    
    // Write new null terminator
    *(end+1) = 0;
    
    return str;
}

void KVConfig::load_from_file(const char *filename)
{
    ost::MutexLock al(cs_);
    
    FILE *fp = fopen(filename, "r");
    if (fp) {
        char buf[1024];
        while (!feof(fp)) {
            char *p = fgets(buf, sizeof(buf), fp);
            p = trim(p);
            if (*p == 0 || *p == '#')
                continue;
            
            char *eqv = strchr(p, '=');
            if (eqv) {
                *eqv = 0;
                char *value = eqv++;
                if (*p != 0) {
                    kvs_[p] = value;
                }
            }
        }
        
        fclose(fp);
    }
}

void KVConfig::save_to(const char *filename)
{
    ost::MutexLock al(cs_);
    
    FILE *fp = fopen(filename, "w");
    if (fp) {
        KVS::const_iterator it;
        for (it = kvs_.begin(); it != kvs_.end(); ++it) {
            fprintf(fp, "%s=%s\n", it->first.c_str(), it->second.c_str());
        }
        
        fclose(fp);
    }
}

void KVConfig::reload()
{
    {
        ost::MutexLock al(cs_);
        kvs_.clear();
    }
    
    load_from_file(filename_.c_str());
    
    std::string sess_fname = filename_ + ".session";
    load_from_file(sess_fname.c_str());
}

void KVConfig::save_as(const char *filename)
{
    save_to(filename);
}

void KVConfig::save()
{
    std::string sess_fname = filename_ + ".session";
    save_to(sess_fname.c_str());
}

const char *KVConfig::get(const char *key, const char *def)
{
    ost::MutexLock al(cs_);
    KVS::const_iterator itf = kvs_.find(key);
    if (itf == kvs_.end())
        return def;
    else
        return itf->first.c_str();
}

void KVConfig::del(const char *key)
{
    ost::MutexLock al(cs_);
    KVS::iterator itf = kvs_.find(key);
    if (itf != kvs_.end())
        kvs_.erase(key);
}

void KVConfig::set(const char *key, const char *value)
{
    ost::MutexLock al(cs_);
    kvs_[key] = value;
}

void KVConfig::set(const char *key, int value)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", value);
    set(key, buf);
}

void KVConfig::set(const char *key, double value)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%.10f", value);
    set(key, buf);
}
