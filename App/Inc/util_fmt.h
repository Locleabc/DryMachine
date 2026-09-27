/**
 * @file    util_fmt.h
 * @brief   Định dạng số thực không cần printf float (tiết kiệm Flash với newlib-nano).
 */
#ifndef UTIL_FMT_H
#define UTIL_FMT_H
#include <stdint.h>
#include <stddef.h>

/* Ghi v với dec chữ số thập phân (0..3) vào buf, trả về con trỏ buf */
char *Fmt_Float(char *buf, size_t size, float v, uint8_t dec);
/* Ghi thời gian giây -> "HH:MM:SS" */
char *Fmt_Time(char *buf, size_t size, uint32_t sec);

#endif
