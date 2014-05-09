//
//  webapi.h
//  mcu
//
//  Created by sunkw on 14-5-7.
//  Copyright (c) 2014年 sunkw. All rights reserved.
//

#ifndef __mcu__webapi__
#define __mcu__webapi__

#include <iostream>
#include <string>
#include <cc++/thread.h>
#include "../gsoap/soapH.h"
#include "Server.h"

class WsServer : ost::Thread
{
    std::string ip_;
    int port_;
    soap soap_;
    bool quit_;
    Server &server_;
    
public:
    WsServer(Server &server, int port, const char *ip = "0.0.0.0");
    ~WsServer();
    
    Server &server()
    {
        return server_;
    }
    
private:
    void run();
};

inline Server &get_server(soap *soap)
{
    return ((WsServer*)soap->user)->server();
}

#endif /* defined(__mcu__webapi__) */
