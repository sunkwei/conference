//
//  webapi.cpp
//  mcu
//
//  Created by sunkw on 14-5-7.
//  Copyright (c) 2014年 sunkw. All rights reserved.
//

#include "webapi.h"
#include "../gsoap/mcu.nsmap"
#include "log.h"

WsServer::WsServer(Server &server, int port, const char *ip) : server_(server)
{
    port_ = port;
    ip_ = ip;
    
    soap_init(&soap_);
    
    if (soap_valid_socket(soap_bind(&soap_, ip_.c_str(), port_, 100))) {
        log("INFO: %s: bind %s:%d OK!\n", __func__, ip, port);
        quit_ = false;
        start();
    }
    else {
        quit_ = true;
        log("ERR: %s: bind %s:%d err!\n", __func__, ip, port);
        throw;
    }
}

WsServer::~WsServer()
{
    if (!quit_) {
        quit_ = true;
        ::soap_closesocket(soap_.master);
        join();
    }
}

void WsServer::run()
{
    soap_.user = this;
    
    while (!quit_) {
        int s = soap_accept(&soap_);
        if (soap_valid_socket(s)) {
            soap_serve(&soap_);
            soap_destroy(&soap_);
            soap_end(&soap_);
        }
    }
    
    soap_done(&soap_);
}
