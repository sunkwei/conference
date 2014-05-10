//
//  Member.cpp
//  mcu
//
//  Created by sunkw on 14-5-7.
//  Copyright (c) 2014年 sunkw. All rights reserved.
//

#include "Member.h"

Member::Member(int confid, int memid) : conf_id_(confid), member_id_(memid)
{
    
}

Member::~Member()
{
    
}

MemberDesc Member::desc()
{
    MemberDesc desc;
    desc.conf_id = conf_id_;
    desc.member_id = member_id_;
    
    return desc;
}
