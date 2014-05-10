//
//  Member.h
//  mcu
//
//  Created by sunkw on 14-5-7.
//  Copyright (c) 2014年 sunkw. All rights reserved.
//
//  会议参与者，拥有唯一 id， media source, sink, stream ....

#ifndef __mcu__Member__
#define __mcu__Member__

#include <iostream>
#include <cc++/thread.h>
#include "Source.h"
#include "Sink.h"
#include "Stream.h"

struct MemberDesc
{
    int conf_id;
    int member_id;
};

class Member
{
    int conf_id_, member_id_;
    
public:
    Member(int confid, int memid);
    ~Member();
    
    MemberDesc desc();
    
};

#endif /* defined(__mcu__Member__) */
