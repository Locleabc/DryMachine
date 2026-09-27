/**
 * @file    util_fmt.c
 * @brief   Không dùng printf float (%f) – tiết kiệm Flash, chạy tốt với MicroLIB.
 */
#include "util_fmt.h"
#include <stdio.h>

char *Fmt_Float(char *buf, size_t size, float v, uint8_t dec)
{
    static const int32_t pow10[] = { 1, 10, 100, 1000 };
    if (dec > 3) dec = 3;
    int neg = (v < 0.0f);
    if (neg) v = -v;
    int32_t scaled = (int32_t)(v * (float)pow10[dec] + 0.5f);
    long ip = (long)(scaled / pow10[dec]);
    long fp = (long)(scaled % pow10[dec]);

    switch (dec) {
    case 0:  snprintf(buf, size, "%s%ld",      neg ? "-" : "", ip);     break;
    case 1:  snprintf(buf, size, "%s%ld.%01ld", neg ? "-" : "", ip, fp); break;
    case 2:  snprintf(buf, size, "%s%ld.%02ld", neg ? "-" : "", ip, fp); break;
    default: snprintf(buf, size, "%s%ld.%03ld", neg ? "-" : "", ip, fp); break;
    }
    return buf;
}

char *Fmt_Time(char *buf, size_t size, uint32_t sec)
{
    snprintf(buf, size, "%02lu:%02lu:%02lu",
             (unsigned long)(sec / 3600), (unsigned long)((sec / 60) % 60), (unsigned long)(sec % 60));
    return buf;
}
