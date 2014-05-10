//
//  Server.h
//  mcu
//
//  Created by sunkw on 14-5-7.
//  Copyright (c) 2014年 sunkw. All rights reserved.
//

#ifndef __mcu__Server__
#define __mcu__Server__

#include <iostream>
#include <list>
#include <vector>
#include <cc++/thread.h>
#include "Conference.h"

class Server
{
    typedef std::list<Conference *> CONFERENCES;
    CONFERENCES conferences_;
    ost::Mutex cs_conferences_;
    
    int next_conf_id_;  // conf_id_ 从 [0..1024)，mod
    ost::Mutex cs_next_conf_id_;
    
public:
    Server();
    ~Server();
    
    size_t get_conference_count();
    
    /** 新建一个 Conference, 返回 conf_id */
    int conference_create();
    int conference_destroy(int conf_id);
    
    /** 返回所有 Conference 的描述 .... */
    std::vector<ConferenceDesc> conference_list();
    
private:
    int next_id();
};

#endif /* defined(__mcu__Server__) */
