/**
 * @file    font5x7.h
 * @brief   Font ASCII 5x7 (ký tự 0x20..0x7E), mỗi ký tự 5 cột, bit0 = hàng trên.
 */
#ifndef FONT5X7_H
#define FONT5X7_H
#include <stdint.h>

#define FONT_W      5
#define FONT_H      7
#define FONT_FIRST  0x20
#define FONT_LAST   0x7E

extern const uint8_t font5x7[][FONT_W];

#endif
