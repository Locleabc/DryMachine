/**
 * @file    drv_ili9341.c
 */
#include "drv_ili9341.h"
#include "font5x7.h"
#include "app_config.h"
#include <stdbool.h>

#define SPI_TIMEOUT   100
#define LINE_BUF_PX   64

static uint16_t s_w = TFT_WIDTH, s_h = TFT_HEIGHT;

static inline void cs_low(void)  { HAL_GPIO_WritePin(TFT_CS_GPIO_Port,  TFT_CS_Pin,  GPIO_PIN_RESET); }
static inline void cs_high(void) { HAL_GPIO_WritePin(TFT_CS_GPIO_Port,  TFT_CS_Pin,  GPIO_PIN_SET); }
static inline void dc_cmd(void)  { HAL_GPIO_WritePin(TFT_DC_GPIO_Port,  TFT_DC_Pin,  GPIO_PIN_RESET); }
static inline void dc_data(void) { HAL_GPIO_WritePin(TFT_DC_GPIO_Port,  TFT_DC_Pin,  GPIO_PIN_SET); }

static void write_cmd(uint8_t cmd)
{
    dc_cmd();
    cs_low();
    HAL_SPI_Transmit(TFT_SPI, &cmd, 1, SPI_TIMEOUT);
    cs_high();
}

static void write_data(const uint8_t *d, uint16_t len)
{
    dc_data();
    cs_low();
    HAL_SPI_Transmit(TFT_SPI, (uint8_t *)d, len, SPI_TIMEOUT);
    cs_high();
}

static void set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    uint8_t d[4];
    write_cmd(0x2A);
    d[0] = x0 >> 8; d[1] = x0 & 0xFF; d[2] = x1 >> 8; d[3] = x1 & 0xFF;
    write_data(d, 4);
    write_cmd(0x2B);
    d[0] = y0 >> 8; d[1] = y0 & 0xFF; d[2] = y1 >> 8; d[3] = y1 & 0xFF;
    write_data(d, 4);
    write_cmd(0x2C);
}

/* Bảng khởi tạo: cmd, số byte data (bit7 = delay 150 ms sau lệnh), data... ; kết thúc 0x00 */
static const uint8_t s_init[] = {
    0xEF, 3, 0x03, 0x80, 0x02,
    0xCF, 3, 0x00, 0xC1, 0x30,
    0xED, 4, 0x64, 0x03, 0x12, 0x81,
    0xE8, 3, 0x85, 0x00, 0x78,
    0xCB, 5, 0x39, 0x2C, 0x00, 0x34, 0x02,
    0xF7, 1, 0x20,
    0xEA, 2, 0x00, 0x00,
    0xC0, 1, 0x23,             /* Power control 1 */
    0xC1, 1, 0x10,             /* Power control 2 */
    0xC5, 2, 0x3E, 0x28,       /* VCOM 1 */
    0xC7, 1, 0x86,             /* VCOM 2 */
    0x36, 1, 0x48,             /* MADCTL */
    0x37, 1, 0x00,
    0x3A, 1, 0x55,             /* 16 bit/pixel */
    0xB1, 2, 0x00, 0x18,       /* Frame rate */
    0xB6, 3, 0x08, 0x82, 0x27, /* Display function */
    0xF2, 1, 0x00,
    0x26, 1, 0x01,             /* Gamma */
    0xE0, 15, 0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08, 0x4E, 0xF1, 0x37, 0x07, 0x10, 0x03, 0x0E, 0x09, 0x00,
    0xE1, 15, 0x00, 0x0E, 0x14, 0x03, 0x11, 0x07, 0x31, 0xC1, 0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F,
    0x11, 0x80,                /* Sleep out + delay */
    0x29, 0x80,                /* Display on + delay */
    0x00
};

void ILI9341_Init(void)
{
    cs_high();
    HAL_GPIO_WritePin(TFT_RST_GPIO_Port, TFT_RST_Pin, GPIO_PIN_RESET);
    HAL_Delay(20);
    HAL_GPIO_WritePin(TFT_RST_GPIO_Port, TFT_RST_Pin, GPIO_PIN_SET);
    HAL_Delay(150);

    const uint8_t *p = s_init;
    while (*p) {
        uint8_t cmd = *p++;
        uint8_t n   = *p++;
        write_cmd(cmd);
        if (n & 0x7F) { write_data(p, n & 0x7F); p += (n & 0x7F); }
        if (n & 0x80) HAL_Delay(150);
    }
    ILI9341_SetRotation(1);
}

void ILI9341_SetRotation(uint8_t rot)
{
    static const uint8_t madctl[4] = { 0x48, 0x28, 0x88, 0xE8 };
    rot &= 3;
    write_cmd(0x36);
    write_data(&madctl[rot], 1);
    if (rot & 1) { s_w = TFT_WIDTH;  s_h = TFT_HEIGHT; }
    else         { s_w = TFT_HEIGHT; s_h = TFT_WIDTH;  }
}

void ILI9341_FillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
{
    if (x >= (int16_t)s_w || y >= (int16_t)s_h || w <= 0 || h <= 0) return;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > (int16_t)s_w) w = (int16_t)(s_w - x);
    if (y + h > (int16_t)s_h) h = (int16_t)(s_h - y);
    if (w <= 0 || h <= 0) return;

    uint8_t buf[LINE_BUF_PX * 2];
    for (int i = 0; i < LINE_BUF_PX; i++) { buf[2 * i] = color >> 8; buf[2 * i + 1] = color & 0xFF; }

    set_window((uint16_t)x, (uint16_t)y, (uint16_t)(x + w - 1), (uint16_t)(y + h - 1));
    uint32_t total = (uint32_t)w * (uint32_t)h;
    dc_data();
    cs_low();
    while (total) {
        uint16_t n = (total > LINE_BUF_PX) ? LINE_BUF_PX : (uint16_t)total;
        HAL_SPI_Transmit(TFT_SPI, buf, (uint16_t)(n * 2), SPI_TIMEOUT);
        total -= n;
    }
    cs_high();
}

void ILI9341_FillScreen(uint16_t color)
{
    ILI9341_FillRect(0, 0, (int16_t)s_w, (int16_t)s_h, color);
}

void ILI9341_DrawPixel(int16_t x, int16_t y, uint16_t color)
{
    ILI9341_FillRect(x, y, 1, 1, color);
}

void ILI9341_DrawHLine(int16_t x, int16_t y, int16_t w, uint16_t color)
{
    ILI9341_FillRect(x, y, w, 1, color);
}

void ILI9341_DrawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
{
    ILI9341_FillRect(x, y, w, 1, color);
    ILI9341_FillRect(x, (int16_t)(y + h - 1), w, 1, color);
    ILI9341_FillRect(x, y, 1, h, color);
    ILI9341_FillRect((int16_t)(x + w - 1), y, 1, h, color);
}

/* Vẽ 1 ký tự trong ô (6*scale) x (8*scale), đẩy cả ô trong 1 lần set_window */
void ILI9341_DrawChar(int16_t x, int16_t y, char c, uint16_t fg, uint16_t bg, uint8_t scale)
{
    if (scale == 0) scale = 1;
    if (scale > 8) scale = 8;
    if ((uint8_t)c < FONT_FIRST || (uint8_t)c > FONT_LAST) c = '?';
    const uint8_t *glyph = font5x7[(uint8_t)c - FONT_FIRST];

    int16_t cw = (int16_t)(6 * scale), ch = (int16_t)(8 * scale);
    if (x < 0 || y < 0 || x + cw > (int16_t)s_w || y + ch > (int16_t)s_h) return;

    set_window((uint16_t)x, (uint16_t)y, (uint16_t)(x + cw - 1), (uint16_t)(y + ch - 1));
    dc_data();
    cs_low();
    uint8_t line[6 * 8 * 2];                /* 1 hàng pixel, scale tối đa 8 */
    for (int row = 0; row < 8; row++) {
        uint16_t idx = 0;
        for (int col = 0; col < 6; col++) {
            bool on = (col < 5) && (row < 7) && (glyph[col] & (1u << row));
            uint16_t px = on ? fg : bg;
            for (int s = 0; s < scale; s++) {
                line[idx++] = (uint8_t)(px >> 8);
                line[idx++] = (uint8_t)(px & 0xFF);
            }
        }
        for (int s = 0; s < scale; s++) {
            HAL_SPI_Transmit(TFT_SPI, line, idx, SPI_TIMEOUT);
        }
    }
    cs_high();
}

void ILI9341_DrawString(int16_t x, int16_t y, const char *s, uint16_t fg, uint16_t bg, uint8_t scale)
{
    while (*s) {
        ILI9341_DrawChar(x, y, *s++, fg, bg, scale);
        x = (int16_t)(x + 6 * scale);
    }
}
