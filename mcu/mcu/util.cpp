//
//  util.cpp
//  mcu
//
//  Created by sunkw on 14-5-7.
//  Copyright (c) 2014年 sunkw. All rights reserved.
//

#include "util.h"

static double _begin = 0.0;

namespace {
    class Init
    {
    public:
        Init()
        {
            _begin = util_now();
        }
    };
    
    static Init _init;
};

double util_now()
{
    struct timeval tv;
    gettimeofday(&tv, 0);
    
    return tv.tv_sec + tv.tv_usec / 1000000.0;
}

double util_uptime()
{
    return util_now() - _begin;
}
