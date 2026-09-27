/**
 * @file    ui_edit.c
 * @brief   Màn hình nhập số từng chữ số, từ trái sang phải.
 *          UP/DOWN: tăng/giảm chữ số đang chọn (0..9 vòng)   ENTER: sang chữ số kế / lưu ở chữ số cuối
 *          EXIT: huỷ, quay về màn hình trước
 */
#include "ui_internal.h"

#define EDIT_MAX   20
#define Y_LABEL    48
#define Y_TEXT     92
#define Y_POS      160

static struct {
    const char    *title;
    const char    *label;
    char           text[EDIT_MAX + 1];
    uint8_t        pos[EDIT_MAX];       /* vị trí các chữ số trong text */
    uint8_t        count, cur;
    ui_edit_done_t done;
    ui_scr_t       back;
    uint8_t        scale;
} e;

void ui_edit_begin(const char *title, const char *label, const char *text,
                   ui_edit_done_t done, ui_scr_t back)
{
    memset(&e, 0, sizeof(e));
    e.title = title;
    e.label = label;
    snprintf(e.text, sizeof(e.text), "%s", text);
    for (uint8_t i = 0; e.text[i] && e.count < EDIT_MAX; i++) {
        if (e.text[i] >= '0' && e.text[i] <= '9') e.pos[e.count++] = i;
    }
    size_t len = strlen(e.text);
    e.scale = (len <= 5) ? 6 : (len <= 8) ? 4 : 3;
    e.done = done;
    e.back = back;
    ui_goto(SCR_EDIT);
}

static void draw_static(void)
{
    /* tiêu đề riêng của lần nhập ghi đè tiêu đề chung "NHAP SO" */
    ILI9341_FillRect(0, 0, 200, UI_HEAD_H, UC_HEAD);
    ILI9341_DrawString(8, 6, e.title ? e.title : "NHAP SO", C_WHITE, UC_HEAD, 2);
    w_text_center(Y_LABEL, e.label ? e.label : "", 24, UC_LABEL, UC_BG, 2);
}

static void draw_values(bool full)
{
    (void)full;
    size_t len = strlen(e.text);
    int16_t cw = (int16_t)(6 * e.scale);
    int16_t x  = (int16_t)((TFT_WIDTH - (int16_t)len * cw) / 2);
    for (size_t i = 0; i < len; i++) {
        bool is_digit = (e.text[i] >= '0' && e.text[i] <= '9');
        bool is_cur   = (e.count > 0 && i == e.pos[e.cur]);
        uint16_t bg = is_cur ? UC_CURSOR : UC_BG;
        uint16_t fg = is_cur ? C_BLACK : (is_digit ? UC_VALUE : UC_LABEL);
        ILI9341_DrawChar((int16_t)(x + (int16_t)i * cw), Y_TEXT, e.text[i], fg, bg, e.scale);
    }
    /* gạch chân chữ số đang chọn */
    ILI9341_FillRect(0, (int16_t)(Y_TEXT + 8 * e.scale + 2), TFT_WIDTH, 4, UC_BG);
    if (e.count) ILI9341_FillRect((int16_t)(x + e.pos[e.cur] * cw), (int16_t)(Y_TEXT + 8 * e.scale + 2), cw, 4, UC_CURSOR);

    char a[24];
    snprintf(a, sizeof(a), "Chu so %u / %u", (unsigned)(e.cur + 1), (unsigned)e.count);
    w_text_center(Y_POS, a, 16, UC_LABEL, UC_BG, 2);
}

static void finish(void)
{
    uint8_t d[EDIT_MAX];
    for (uint8_t i = 0; i < e.count; i++) d[i] = (uint8_t)(e.text[e.pos[i]] - '0');
    if (e.done) e.done(d, e.count);          /* callback tự chuyển màn hình */
    else ui_goto(e.back);
}

static void key(ui_key_t k, ui_press_t p)
{
    if (e.count == 0) { ui_goto(e.back); return; }
    char *c = &e.text[e.pos[e.cur]];

    switch (k) {
    case UI_KEY_UP:
        if (p != UI_PRESS_LONG) *c = (*c >= '9') ? '0' : (char)(*c + 1);
        break;
    case UI_KEY_DOWN:
        if (p != UI_PRESS_LONG) *c = (*c <= '0') ? '9' : (char)(*c - 1);
        break;
    case UI_KEY_ENTER:
        if (p != UI_PRESS_SHORT) break;
        if (e.cur + 1 < e.count) e.cur++;
        else finish();
        break;
    case UI_KEY_EXIT:
        if (p == UI_PRESS_SHORT) {
            UI_Message("DA HUY");
            ui_goto(e.back);
        }
        break;
    }
}

static const char *hint(void)
{
    return (e.cur + 1 < e.count) ? "UP/DN: doi so  ENTER: tiep  EXIT: huy"
                                 : "UP/DN: doi so  ENTER: LUU  EXIT: huy";
}

const ui_screen_t scr_edit = {
    .title = "", .page = -1, .refresh_ms = 1000,
    .draw_static = draw_static, .draw_values = draw_values, .key = key, .hint = hint,
};
