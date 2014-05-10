//
//  Conference.h
//  mcu
//
//  Created by sunkw on 14-5-7.
//  Copyright (c) 2014年 sunkw. All rights reserved.
//
//  会议，包括唯一 id, + members ....


#ifndef __mcu__Conference__
#define __mcu__Conference__

#include <iostream>
#include <list>
#include <vector>
#include <cc++/thread.h>
#include "Member.h"

struct ConferenceDesc
{
    int conf_id;
};

class Conference
{
    typedef std::list<Member*> MEMBERS;
    MEMBERS members_;
    ost::Mutex cs_members_;
    
    int conf_id_;
    
    int next_memeber_id_;
    ost::Mutex cs_next_member_id_;
    
public:
    Conference(int conf_id);
    ~Conference();
    
    ConferenceDesc desc();
    
    // 创建 member，返回 member_id
    int member_create();
    int member_destroy(int member_id);
    
    // 返回所有 member 的描述
    std::vector<MemberDesc> member_list();
    
private:
    int next_id();
};

#endif /* defined(__mcu__Conference__) */
