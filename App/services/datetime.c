/**
 * @file    datetime.c
 */
#include "datetime.h"

static bool is_leap(uint16_t y)
{
    return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
}

static uint8_t days_in_month(uint16_t y, uint8_t m)
{
    static const uint8_t d[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    return (m == 2 && is_leap(y)) ? 29 : d[m - 1];
}

bool DateTime_IsValid(const datetime_t *dt)
{
    if (dt->year < 2000 || dt->year > 2099) return false;
    if (dt->mon < 1 || dt->mon > 12) return false;
    if (dt->day < 1 || dt->day > days_in_month(dt->year, dt->mon)) return false;
    return dt->hour < 24 && dt->min < 60 && dt->sec < 60;
}

uint32_t DateTime_ToEpoch(const datetime_t *dt)
{
    uint32_t days = 0;
    for (uint16_t y = 2000; y < dt->year; y++) days += is_leap(y) ? 366 : 365;
    for (uint8_t m = 1; m < dt->mon; m++) days += days_in_month(dt->year, m);
    days += (uint32_t)(dt->day - 1);
    return days * 86400UL + dt->hour * 3600UL + dt->min * 60UL + dt->sec;
}

void DateTime_FromEpoch(uint32_t epoch, datetime_t *dt)
{
    uint32_t days = epoch / 86400UL;
    uint32_t rem  = epoch % 86400UL;
    dt->hour = (uint8_t)(rem / 3600);
    dt->min  = (uint8_t)((rem / 60) % 60);
    dt->sec  = (uint8_t)(rem % 60);

    uint16_t y = 2000;
    for (;;) {
        uint16_t n = is_leap(y) ? 366 : 365;
        if (days < n) break;
        days -= n;
        y++;
    }
    uint8_t m = 1;
    for (;;) {
        uint8_t n = days_in_month(y, m);
        if (days < n) break;
        days -= n;
        m++;
    }
    dt->year = y;
    dt->mon  = m;
    dt->day  = (uint8_t)(days + 1);
}
