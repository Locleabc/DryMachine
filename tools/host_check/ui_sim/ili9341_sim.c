/* ILI9341 giả lập trên PC: vẽ vào framebuffer RGB565 320x240, xuất file PPM.
 * Cùng API với App/drivers/ili9341/ili9341.h để chạy nguyên code ui/ trên PC. */
#include "ili9341.h"
#include "font5x7.h"
#include <stdio.h>

static uint16_t fb[TFT_HEIGHT][TFT_WIDTH];
static int s_w = TFT_WIDTH, s_h = TFT_HEIGHT;

void ILI9341_Init(const ili9341_cfg_t *cfg) { (void)cfg; }
void ILI9341_SetRotation(uint8_t rot) { (void)rot; }

void ILI9341_FillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
{
    for (int j = y; j < y + h; j++)
        for (int i = x; i < x + w; i++)
            if (i >= 0 && j >= 0 && i < s_w && j < s_h) fb[j][i] = color;
}
void ILI9341_FillScreen(uint16_t c) { ILI9341_FillRect(0, 0, TFT_WIDTH, TFT_HEIGHT, c); }
void ILI9341_DrawPixel(int16_t x, int16_t y, uint16_t c) { ILI9341_FillRect(x, y, 1, 1, c); }
void ILI9341_DrawHLine(int16_t x, int16_t y, int16_t w, uint16_t c) { ILI9341_FillRect(x, y, w, 1, c); }
void ILI9341_DrawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c)
{
    ILI9341_FillRect(x, y, w, 1, c); ILI9341_FillRect(x, y + h - 1, w, 1, c);
    ILI9341_FillRect(x, y, 1, h, c); ILI9341_FillRect(x + w - 1, y, 1, h, c);
}

static int s_overflow;
void ILI9341_DrawChar(int16_t x, int16_t y, char c, uint16_t fg, uint16_t bg, uint8_t scale)
{
    if (scale == 0) scale = 1;
    if ((uint8_t)c < FONT_FIRST || (uint8_t)c > FONT_LAST) c = '?';
    const uint8_t *g = font5x7[(uint8_t)c - FONT_FIRST];
    if (x < 0 || y < 0 || x + 6 * scale > s_w || y + 8 * scale > s_h) { s_overflow++; return; }
    for (int col = 0; col < 6; col++)
        for (int row = 0; row < 8; row++) {
            int on = col < 5 && row < 7 && (g[col] & (1u << row));
            ILI9341_FillRect((int16_t)(x + col * scale), (int16_t)(y + row * scale), scale, scale, on ? fg : bg);
        }
}
void ILI9341_DrawString(int16_t x, int16_t y, const char *s, uint16_t fg, uint16_t bg, uint8_t scale)
{
    while (*s) { ILI9341_DrawChar(x, y, *s++, fg, bg, scale); x = (int16_t)(x + 6 * scale); }
}

static int bx, by, bw, bh, bi;
int ILI9341_BlitBegin(int16_t x, int16_t y, int16_t w, int16_t h)
{
    if (x < 0 || y < 0 || w <= 0 || h <= 0 || x + w > s_w || y + h > s_h) { s_overflow++; return 0; }
    bx = x; by = y; bw = w; bh = h; bi = 0;
    return 1;
}
void ILI9341_BlitPixels(const uint16_t *px, uint16_t n)
{
    while (n--) {
        if (bi < bw * bh) fb[by + bi / bw][bx + bi % bw] = *px;
        px++; bi++;
    }
}
void ILI9341_BlitEnd(void) { if (bi != bw * bh) s_overflow++; }

int Sim_Overflow(void) { int o = s_overflow; s_overflow = 0; return o; }

void Sim_Save(const char *path)
{
    FILE *f = fopen(path, "wb");
    if (!f) return;
    fprintf(f, "P6\n%d %d\n255\n", TFT_WIDTH, TFT_HEIGHT);
    for (int j = 0; j < TFT_HEIGHT; j++)
        for (int i = 0; i < TFT_WIDTH; i++) {
            uint16_t c = fb[j][i];
            unsigned char rgb[3] = { (unsigned char)((c >> 8) & 0xF8), (unsigned char)((c >> 3) & 0xFC),
                                     (unsigned char)((c << 3) & 0xF8) };
            fwrite(rgb, 1, 3, f);
        }
    fclose(f);
}
