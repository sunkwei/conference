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
#include <cc++/thread.h>

class Conference;

class Server
{
    typedef std::list<Conference *> CONFERENCES;
    CONFERENCES conferences_;
    ost::Mutex cs_conferences_;
    
public:
    Server();
    ~Server();
    
    size_t get_conference_count();
    
};

#endif /* defined(__mcu__Server__) */
