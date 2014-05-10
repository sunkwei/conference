//
//  Ref.h
//  mcu
//
//  Created by sunkw on 14-5-10.
//  Copyright (c) 2014年 sunkw. All rights reserved.
//

#ifndef __mcu__Ref__
#define __mcu__Ref__

#include <iostream>
#include <cc++/thread.h>

/// 实现引用计数
class Ref
{
    size_t ref_;
    ost::Mutex cs_;
    
public:
    Ref()
    {
        ref_ = 1;
    }
    
    void addref()
    {
        ost::MutexLock al(cs_);
        ref_++;
    }
    
    void release()
    {
        cs_.enter();
        ref_--;
        cs_.leave();
        
        if (ref_ == 0)
            delete this;
    }
    
protected:
    virtual ~Ref() {}
};

#endif /* defined(__mcu__Ref__) */
