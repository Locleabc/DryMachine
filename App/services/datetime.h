/**
 * @file    datetime.h
 * @brief   Đổi qua lại giữa "số giây kể từ 01/01/2000 00:00:00" và ngày giờ.
 *          Thuần C, dùng cho bộ đếm RTC 32 bit của STM32F1 (đủ tới năm 2136).
 */
#ifndef DATETIME_H
#define DATETIME_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint16_t year;     /* 2000..2099 */
    uint8_t  mon;      /* 1..12 */
    uint8_t  day;      /* 1..31 */
    uint8_t  hour;     /* 0..23 */
    uint8_t  min;      /* 0..59 */
    uint8_t  sec;      /* 0..59 */
} datetime_t;

bool     DateTime_IsValid(const datetime_t *dt);
uint32_t DateTime_ToEpoch(const datetime_t *dt);
void     DateTime_FromEpoch(uint32_t epoch, datetime_t *dt);

#endif
