//
//  Source.h
//  mcu
//
//  Created by sunkw on 14-5-8.
//  Copyright (c) 2014年 sunkw. All rights reserved.
//
//  Source 对应数据源，就是一个 rtp recv，接收 member 上传的媒体数据

#ifndef __mcu__Source__
#define __mcu__Source__

#include <iostream>

class SourceDesc
{
    
};

class Source
{
public:
    Source();
    virtual ~Source();
    
    SourceDesc desc();
};

#endif /* defined(__mcu__Source__) */
