/**
 * @file    ili9341_text.h
 * @brief   Vẽ chữ UTF-8 (tiếng Việt có dấu) bằng font bitmap anti-alias 2 bit/pixel.
 *          Font sinh bởi tools/fontgen/fontgen.py → fonts_vn.c.
 *
 *  Chuỗi trong mã nguồn viết thẳng tiếng Việt, file lưu UTF-8 (dạng dựng sẵn – NFC).
 *  Ký tự không có trong font được vẽ thành '?'.
 */
#ifndef ILI9341_TEXT_H
#define ILI9341_TEXT_H

#include <stdint.h>

typedef struct {
    uint16_t cp;        /* mã Unicode */
    uint16_t off;       /* vị trí bitmap (byte) */
    uint8_t  w, h;      /* kích thước phần có mực */
    int8_t   x, y;      /* vị trí phần có mực trong ô ký tự (y tính từ đỉnh dòng) */
    uint8_t  adv;       /* độ rộng ô ký tự */
} vfont_glyph_t;

typedef struct {
    const uint8_t       *bits;
    const vfont_glyph_t *glyphs;   /* sắp xếp tăng dần theo cp */
    uint16_t             count;
    uint8_t              height;   /* chiều cao dòng (px) */
    uint8_t              baseline; /* đường chân chữ tính từ đỉnh dòng */
    uint8_t              bpp;      /* 2 */
} vfont_t;

typedef enum { TEXT_LEFT = 0, TEXT_CENTER, TEXT_RIGHT } text_align_t;

extern const vfont_t font_vn16;    /* chữ thường + hoa tiếng Việt, cao dòng 23 px */
extern const vfont_t font_num;     /* số lớn: 0-9 . - : / % ° C, cao dòng ~35 px */

int16_t Text_Width(const vfont_t *f, const char *utf8);
/* Vẽ từ (x, y) = góc trên-trái dòng; trả về x sau ký tự cuối */
int16_t Text_Draw(int16_t x, int16_t y, const char *utf8, const vfont_t *f, uint16_t fg, uint16_t bg);
/* Vẽ trong ô rộng w: căn lề, tô nền phần còn lại (xoá chữ cũ), cắt nếu quá dài */
void    Text_Box(int16_t x, int16_t y, int16_t w, const char *utf8, const vfont_t *f,
                 uint16_t fg, uint16_t bg, text_align_t align);
/* Đọc 1 ký tự UTF-8, trả về mã Unicode và dịch con trỏ */
uint16_t Text_NextChar(const char **p);

#endif
