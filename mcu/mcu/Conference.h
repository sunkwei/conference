//
//  Conference.h
//  mcu
//
//  Created by sunkw on 14-5-7.
//  Copyright (c) 2014年 sunkw. All rights reserved.
//
//  会议，包括唯一 id, + members ....
//  媒体传输（Ticker, Filters ...）对象都在 Conference 层级构造，Member 仅仅包含属性信息 ....

#ifndef __mcu__Conference__
#define __mcu__Conference__

#include <iostream>
#include <list>
#include <vector>
#include <cc++/thread.h>
#include "Ref.h"
#include "Member.h"
#include <mediastreamer2/msconference.h>    // audio conference

struct ConferenceDesc
{
    int conf_id;
};

class Conference : public Ref
{
    typedef std::list<Member*> MEMBERS;
    MEMBERS members_;
    ost::Mutex cs_members_;
    
    int conf_id_;
    
    int next_memeber_id_;
    ost::Mutex cs_next_member_id_;

    virtual ~Conference();
    
public:
    Conference(int conf_id);
    
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
