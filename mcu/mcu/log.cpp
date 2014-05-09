//
//  log.cpp
//  mcu
//
//  Created by sunkw on 14-5-7.
//  Copyright (c) 2014年 sunkw. All rights reserved.
//

#include <stdio.h>
#include <stdarg.h>
#include "log.h"

void log_init()
{
    FILE *fp = fopen(LOG_FILENAME, "w");
    if (fp) {
        fprintf(fp, "============== begin ===============\n");
        fclose(fp);
    }
}

void log(const char *fmt, ...)
{
    FILE *fp = fopen(LOG_FILENAME, "at");
    if (fp) {
        char buf[1024];
        va_list args;
        
        va_start(args, fmt);
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);
        
        fprintf(fp, "%s", buf);
        fprintf(stdout, "%s", buf);
        fclose(fp);
    }
}
