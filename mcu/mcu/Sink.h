//
//  Sink.h
//  mcu
//
//  Created by sunkw on 14-5-8.
//  Copyright (c) 2014年 sunkw. All rights reserved.
//
//  Sink 对应着 Member 的接收

#ifndef __mcu__Sink__
#define __mcu__Sink__

#include <iostream>

class SinkDesc
{
    
};

// 一个 Sink 对应一个 rtp sender，将数据发送到 peer
class Sink
{
public:
    Sink();
    virtual ~Sink();
    
    SinkDesc desc();
};

#endif /* defined(__mcu__Sink__) */
