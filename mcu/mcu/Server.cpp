//
//  Server.cpp
//  mcu
//
//  Created by sunkw on 14-5-7.
//  Copyright (c) 2014年 sunkw. All rights reserved.
//

#include "Server.h"
#include "Member.h"
#include "defined.h"

Server::Server()
{
    next_conf_id_ = 0;
}

Server::~Server()
{
    
}

size_t Server::get_conference_count()
{
    ost::MutexLock al(cs_conferences_);
    return conferences_.size();
}

int Server::conference_create()
{
    ost::MutexLock al(cs_conferences_);
    
    int conf_id = next_id();
    
    Conference *conf = new Conference(conf_id);
    conferences_.push_back(conf);
    
    return conf_id;
}

int Server::conference_destroy(int conf_id)
{
    ost::MutexLock al(cs_conferences_);
    
    CONFERENCES::iterator it;
    for (it = conferences_.begin(); it != conferences_.end(); ++it) {
        if ((*it)->desc().conf_id == conf_id) {
            conferences_.erase(it);
            delete (*it);
            break;
        }
    }
    
    return 0;
}

std::vector<ConferenceDesc> Server::conference_list()
{
    std::vector<ConferenceDesc> confs;
    ost::MutexLock al(cs_conferences_);
    
    for (CONFERENCES::iterator it = conferences_.begin(); it != conferences_.end(); ++it) {
        confs.push_back((*it)->desc());
    }
    
    return confs;
}

int Server::next_id()
{
    ost::MutexLock al(cs_next_conf_id_);
    int id = next_conf_id_;
    
    next_conf_id_++;
    next_conf_id_ %= (D_MAX_CONF_ID+1);
    
    return id;
}
