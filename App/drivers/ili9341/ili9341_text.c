/**
 * @file    ili9341_text.c
 */
#include "ili9341_text.h"
#include "ili9341.h"

#define MAX_ADV   64          /* ô ký tự rộng nhất (px) */

uint16_t Text_NextChar(const char **p)
{
    const uint8_t *s = (const uint8_t *)*p;
    uint16_t cp;
    if (s[0] < 0x80)                                  { cp = s[0]; *p += 1; }
    else if ((s[0] & 0xE0) == 0xC0 && s[1])           { cp = (uint16_t)(((s[0] & 0x1F) << 6) | (s[1] & 0x3F)); *p += 2; }
    else if ((s[0] & 0xF0) == 0xE0 && s[1] && s[2])   { cp = (uint16_t)(((s[0] & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F)); *p += 3; }
    else                                              { cp = '?'; *p += 1; }
    return cp;
}

static const vfont_glyph_t *find(const vfont_t *f, uint16_t cp)
{
    int lo = 0, hi = (int)f->count - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        uint16_t c = f->glyphs[mid].cp;
        if (c == cp) return &f->glyphs[mid];
        if (c < cp) lo = mid + 1; else hi = mid - 1;
    }
    return (cp != '?') ? find(f, '?') : 0;
}

static uint16_t blend(uint16_t fg, uint16_t bg, uint8_t a)   /* a = 0..3 */
{
    if (a == 0) return bg;
    if (a == 3) return fg;
    uint16_t r = (uint16_t)((((fg >> 11) & 0x1F) * a + ((bg >> 11) & 0x1F) * (3 - a)) / 3);
    uint16_t g = (uint16_t)((((fg >> 5)  & 0x3F) * a + ((bg >> 5)  & 0x3F) * (3 - a)) / 3);
    uint16_t b = (uint16_t)((( fg        & 0x1F) * a + ( bg        & 0x1F) * (3 - a)) / 3);
    return (uint16_t)((r << 11) | (g << 5) | b);
}

/* Vẽ 1 ô ký tự rộng g->adv, cao f->height; trả về độ rộng đã vẽ */
static int16_t draw_glyph(int16_t x, int16_t y, const vfont_t *f, const vfont_glyph_t *g,
                          uint16_t fg, uint16_t bg, int16_t max_w)
{
    int16_t w = g->adv;
    if (w > max_w) w = max_w;
    if (w > MAX_ADV) w = MAX_ADV;
    if (w <= 0) return 0;
    if (!ILI9341_BlitBegin(x, y, w, f->height)) return w;

    uint16_t pal[4] = { bg, blend(fg, bg, 1), blend(fg, bg, 2), fg };
    uint16_t row[MAX_ADV];
    const uint8_t *bits = f->bits + g->off;

    for (int16_t ry = 0; ry < f->height; ry++) {
        for (int16_t i = 0; i < w; i++) row[i] = bg;
        int16_t gy = (int16_t)(ry - g->y);
        if (gy >= 0 && gy < g->h) {
            for (int16_t gx = 0; gx < g->w; gx++) {
                int16_t cx = (int16_t)(g->x + gx);
                if (cx < 0 || cx >= w) continue;
                uint32_t idx = (uint32_t)gy * g->w + (uint32_t)gx;
                uint8_t  a = (uint8_t)((bits[idx >> 2] >> (6 - 2 * (idx & 3))) & 3);
                if (a) row[cx] = pal[a];
            }
        }
        ILI9341_BlitPixels(row, (uint16_t)w);
    }
    ILI9341_BlitEnd();
    return w;
}

int16_t Text_Width(const vfont_t *f, const char *s)
{
    int16_t w = 0;
    while (*s) {
        const vfont_glyph_t *g = find(f, Text_NextChar(&s));
        if (g) w = (int16_t)(w + g->adv);
    }
    return w;
}

int16_t Text_Draw(int16_t x, int16_t y, const char *s, const vfont_t *f, uint16_t fg, uint16_t bg)
{
    while (*s) {
        const vfont_glyph_t *g = find(f, Text_NextChar(&s));
        if (!g) continue;
        if (x + g->adv > TFT_WIDTH) break;
        x = (int16_t)(x + draw_glyph(x, y, f, g, fg, bg, g->adv));
    }
    return x;
}

void Text_Box(int16_t x, int16_t y, int16_t w, const char *s, const vfont_t *f,
              uint16_t fg, uint16_t bg, text_align_t align)
{
    int16_t tw = Text_Width(f, s);
    int16_t pad = 0;
    if (tw < w) {
        if (align == TEXT_CENTER)     pad = (int16_t)((w - tw) / 2);
        else if (align == TEXT_RIGHT) pad = (int16_t)(w - tw);
    }
    if (pad > 0) ILI9341_FillRect(x, y, pad, f->height, bg);

    int16_t cx = (int16_t)(x + pad), end = (int16_t)(x + w);
    while (*s && cx < end) {
        const vfont_glyph_t *g = find(f, Text_NextChar(&s));
        if (!g) continue;
        if (cx + g->adv > end) break;                         /* không vẽ ký tự bị cắt */
        cx = (int16_t)(cx + draw_glyph(cx, y, f, g, fg, bg, (int16_t)(end - cx)));
    }
    if (cx < end) ILI9341_FillRect(cx, y, (int16_t)(end - cx), f->height, bg);
}
