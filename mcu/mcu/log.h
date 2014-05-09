//
//  log.h
//  mcu
//
//  Created by sunkw on 14-5-7.
//  Copyright (c) 2014年 sunkw. All rights reserved.
//

#ifndef __mcu__log__
#define __mcu__log__

#include <iostream>

#define LOG_FILENAME "mcu.log"

void log_init();
void log(const char *fmt, ...);

#endif /* defined(__mcu__log__) */
