//
//  Conference.cpp
//  mcu
//
//  Created by sunkw on 14-5-7.
//  Copyright (c) 2014年 sunkw. All rights reserved.
//

#include "Conference.h"

Conference::Conference(int conf_id) : conf_id_(conf_id)
{
    next_memeber_id_ = 0;
}

Conference::~Conference()
{
    
}

int Conference::member_create()
{
    ost::MutexLock al(cs_members_);
    
    int id = next_id();
    
    Member *member = new Member(conf_id_, id);
    members_.push_back(member);
    
    return id;
}

int Conference::member_destroy(int member_id)
{
    ost::MutexLock al(cs_members_);
    
    MEMBERS::iterator it;
    for (it = members_.begin(); it != members_.end(); ++it) {
        if ((*it)->desc().member_id == member_id) {
            members_.erase(it);
            (*it)->release();
            break;
        }
    }
    
    return 0;
}

ConferenceDesc Conference::desc()
{
    ost::MutexLock al(cs_members_);
    ConferenceDesc desc;
    desc.conf_id = conf_id_;
    
    // TODO: 复制 ...
    
    return desc;
}

std::vector<MemberDesc> Conference::member_list()
{
    ost::MutexLock al(cs_members_);
    std::vector<MemberDesc> members;
    
    for (MEMBERS::iterator it = members_.begin(); it != members_.end(); ++it) {
        members.push_back((*it)->desc());
    }
    
    return members;
}

int Conference::next_id()
{
    ost::MutexLock al(cs_next_member_id_);
    
    int id = next_memeber_id_;
    
    next_memeber_id_++;
    
    return id;
}
