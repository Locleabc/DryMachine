/**
 * @file    log.c
 */
#include "log.h"
#include <stdarg.h>
#include <stdio.h>

static log_write_fn s_write;

void Log_Init(log_write_fn write)
{
    s_write = write;
}

void Log_Printf(const char *fmt, ...)
{
    if (!s_write) return;
    char buf[128];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf) - 2, fmt, ap);
    va_end(ap);
    if (n < 0) return;
    if (n > (int)sizeof(buf) - 3) n = (int)sizeof(buf) - 3;
    buf[n++] = '\r';
    buf[n++] = '\n';
    s_write(buf, (uint16_t)n);
}
