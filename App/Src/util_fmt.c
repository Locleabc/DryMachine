/**
 * @file    util_fmt.c
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
    int32_t ip = scaled / pow10[dec];
    int32_t fp = scaled % pow10[dec];
    if (dec == 0) snprintf(buf, size, "%s%ld", neg ? "-" : "", (long)ip);
    else          snprintf(buf, size, "%s%ld.%0*ld", neg ? "-" : "", (long)ip, dec, (long)fp);
    return buf;
}

char *Fmt_Time(char *buf, size_t size, uint32_t sec)
{
    snprintf(buf, size, "%02lu:%02lu:%02lu",
             (unsigned long)(sec / 3600), (unsigned long)((sec / 60) % 60), (unsigned long)(sec % 60));
    return buf;
}
