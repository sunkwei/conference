//
//  Server.cpp
//  mcu
//
//  Created by sunkw on 14-5-7.
//  Copyright (c) 2014年 sunkw. All rights reserved.
//

#include "Server.h"

Server::Server()
{
    
}

Server::~Server()
{
    
}

size_t Server::get_conference_count()
{
    ost::MutexLock al(cs_conferences_);
    return conferences_.size();
}
