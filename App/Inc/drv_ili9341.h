/**
 * @file    drv_ili9341.h
 * @brief   Driver TFT ILI9341 qua SPI (RGB565), hướng ngang 320x240.
 */
#ifndef DRV_ILI9341_H
#define DRV_ILI9341_H

#include <stdint.h>

#define TFT_WIDTH    320
#define TFT_HEIGHT   240

/* Màu RGB565 */
#define RGB565(r,g,b)  ((uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3)))
#define C_BLACK      0x0000
#define C_WHITE      0xFFFF
#define C_RED        0xF800
#define C_GREEN      0x07E0
#define C_BLUE       0x001F
#define C_YELLOW     0xFFE0
#define C_CYAN       0x07FF
#define C_ORANGE     0xFD20
#define C_GRAY       0x8410
#define C_DARKGRAY   0x39E7
#define C_NAVY       0x000F

void ILI9341_Init(void);
void ILI9341_SetRotation(uint8_t rot);           /* 0..3; 1 = ngang */
void ILI9341_FillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
void ILI9341_FillScreen(uint16_t color);
void ILI9341_DrawPixel(int16_t x, int16_t y, uint16_t color);
void ILI9341_DrawHLine(int16_t x, int16_t y, int16_t w, uint16_t color);
void ILI9341_DrawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
/* Vẽ ký tự/chuỗi, scale = 1,2,3.. (ô ký tự 6*scale x 8*scale), nền bg luôn được tô */
void ILI9341_DrawChar(int16_t x, int16_t y, char c, uint16_t fg, uint16_t bg, uint8_t scale);
void ILI9341_DrawString(int16_t x, int16_t y, const char *s, uint16_t fg, uint16_t bg, uint8_t scale);

#endif
