//
//  main.cpp
//  mcu
//
//  Created by sunkw on 14-5-6.
//  Copyright (c) 2014年 sunkw. All rights reserved.
//

#include <iostream>
#include <cc++/thread.h>
#include <mediastreamer2/mediastream.h>
#include <ortp/ortp.h>
#include "webapi.h"
#include <signal.h>
#include "Server.h"

static bool _quit = false;

static void signal_handle(int c)
{
    _quit = true;
}

int main(int argc, const char * argv[])
{
    signal(SIGINT, signal_handle);
    
    ortp_init();
    ms_init();
    
    Server server;

    WsServer ws(server, 9900);
    
    while (!_quit) {
        ost::Thread::sleep(1000);
    }
    
    return 0;
}
