#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "log.h"

void log_init()
{
}

void log_uninit()
{
}

void log(const char *fmt, ...)
{
	char buf[1024];
	va_list mark;
	
	va_start(mark, fmt);
	vsnprintf(buf, sizeof(buf), fmt, mark);
	va_end(mark);

	fprintf(stdout, buf);
}
