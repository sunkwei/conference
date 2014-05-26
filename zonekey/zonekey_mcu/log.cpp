#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "log.h"
#include <time.h>
#include <string.h>
#include <cc++/socket.h>

static const char *_filename = "zonekey_mcu.log";

void log_init()
{
	// 根据启动日期，生成日志文件
	time_t now = time(0);
	struct tm *ptm = localtime(&now);

	char filename[128];
	
	snprintf(filename, sizeof(filename), "%04d-%02d-%02d.log", ptm->tm_year+1900, ptm->tm_mon+1, ptm->tm_mday);

	FILE *fp = fopen(filename, "w");
	if (fp) {
		log(" zonekey_mcu log inited ....\n");
		fclose(fp);

		_filename = strdup(filename);
	}
}

void log_uninit()
{
}

#ifdef WIN32
#else
#  define GetTickCount() 0
#endif

void log(const char *fmt, ...)
{
	char buf[1024];
	va_list mark;
	time_t now = time(0);
	struct tm *ptm = localtime(&now);
	
	va_start(mark, fmt);
	vsnprintf(buf, sizeof(buf), fmt, mark);
	va_end(mark);

	char buf2[1024+64];
	snprintf(buf2, sizeof(buf2), "%02d-%02d %02d:%02d:%02d.%03d: %s", 
		ptm->tm_mon+1, ptm->tm_mday,
		ptm->tm_hour, ptm->tm_min, ptm->tm_sec, GetTickCount()%1000,
		buf);
	fprintf(stdout, buf2);

	FILE *fp = fopen(_filename, "at");
	if (fp) {
		fprintf(fp, buf2);
		fclose(fp);
	}
	//fflush(stdout);
}
