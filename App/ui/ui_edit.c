/**
 * @file    ui_edit.c
 * @brief   Màn hình nhập số từng chữ số, từ trái sang phải (số lớn font_num).
 *          text: chữ số '0'..'9' là ô nhập, ký tự khác giữ nguyên, '\n' xuống dòng.
 *          UP/DOWN: tăng/giảm chữ số đang chọn (0..9 vòng)   ENTER: sang chữ số kế / lưu ở chữ số cuối
 *          EXIT: huỷ, quay về màn hình trước
 */
#include "ui_internal.h"

#define EDIT_MAX   24            /* byte */
#define DIG_MAX    12            /* số chữ số tối đa */
#define Y_LABEL    38
#define Y_TEXT1    72            /* dòng số đầu tiên */
#define LINE_GAP   8
#define CURSOR_H   4

static struct {
    const char    *title;
    const char    *label;
    char           text[EDIT_MAX + 1];
    uint8_t        pos[DIG_MAX];       /* vị trí byte của các chữ số trong text */
    uint8_t        count, cur;
    ui_edit_done_t done;
    ui_scr_t       back;
} e;

void ui_edit_begin(const char *title, const char *label, const char *text,
                   ui_edit_done_t done, ui_scr_t back)
{
    memset(&e, 0, sizeof(e));
    e.title = title;
    e.label = label;
    snprintf(e.text, sizeof(e.text), "%s", text);
    for (uint8_t i = 0; e.text[i] && e.count < DIG_MAX; i++) {
        if (e.text[i] >= '0' && e.text[i] <= '9') e.pos[e.count++] = i;
    }
    e.done = done;
    e.back = back;
    ui_goto(SCR_EDIT);
}

static void draw_static(void)
{
    /* tiêu đề riêng của lần nhập ghi đè tiêu đề chung */
    Text_Box(8, 2, 210, e.title ? e.title : "NHẬP SỐ", F_TXT, C_WHITE, g_ui.sim_on ? UC_SIMHEAD : UC_HEAD, TEXT_LEFT);
    w_text(0, Y_LABEL, TFT_WIDTH, e.label ? e.label : "", UC_LABEL, UC_BG, TEXT_CENTER);
}

/* Vẽ 1 dòng (từ byte `start` tới '\n' hoặc hết chuỗi), căn giữa */
static void draw_line(uint8_t start, uint8_t end, int16_t y)
{
    char one[4];
    int16_t w = 0;
    /* đo độ rộng dòng */
    for (const char *p = &e.text[start]; p < &e.text[end];) {
        const char *q = p;
        Text_NextChar(&q);
        memcpy(one, p, (size_t)(q - p)); one[q - p] = '\0';
        w = (int16_t)(w + Text_Width(F_NUM, one));
        p = q;
    }
    int16_t x = (int16_t)((TFT_WIDTH - w) / 2);
    ILI9341_FillRect(0, y, x, font_num.height + CURSOR_H + 2, UC_BG);
    for (const char *p = &e.text[start]; p < &e.text[end];) {
        const char *q = p;
        Text_NextChar(&q);
        memcpy(one, p, (size_t)(q - p)); one[q - p] = '\0';
        uint8_t byte_pos = (uint8_t)(p - e.text);
        bool is_digit = (*p >= '0' && *p <= '9');
        bool is_cur   = (e.count > 0 && byte_pos == e.pos[e.cur]);
        int16_t cw = Text_Width(F_NUM, one);
        Text_Draw(x, y, one, F_NUM, is_cur ? C_BLACK : (is_digit ? UC_VALUE : UC_LABEL),
                  is_cur ? UC_CURSOR : UC_BG);
        ILI9341_FillRect(x, (int16_t)(y + font_num.height), cw, 2, UC_BG);
        ILI9341_FillRect(x, (int16_t)(y + font_num.height + 2), cw, CURSOR_H, is_cur ? UC_CURSOR : UC_BG);
        x = (int16_t)(x + cw);
        p = q;
    }
    ILI9341_FillRect(x, y, (int16_t)(TFT_WIDTH - x), font_num.height + CURSOR_H + 2, UC_BG);
}

static void draw_values(bool full)
{
    (void)full;
    int16_t y = Y_TEXT1;
    uint8_t start = 0, i = 0;
    for (;; i++) {
        if (e.text[i] == '\n' || e.text[i] == '\0') {
            draw_line(start, i, y);
            y = (int16_t)(y + font_num.height + CURSOR_H + LINE_GAP);
            if (e.text[i] == '\0') break;
            start = (uint8_t)(i + 1);
        }
    }
    char a[32];
    snprintf(a, sizeof(a), "Chữ số %u / %u", (unsigned)(e.cur + 1), (unsigned)e.count);
    w_text(0, (int16_t)(UI_FOOT_Y - LINE_H - 4), TFT_WIDTH, a, UC_LABEL, UC_BG, TEXT_CENTER);
}

static void finish(void)
{
    uint8_t d[DIG_MAX];
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
            UI_Message("Đã huỷ");
            ui_goto(e.back);
        }
        break;
    }
}

static const char *hint(void)
{
    return (e.cur + 1 < e.count) ? "UP/DOWN: đổi số · ENTER: tiếp · EXIT: huỷ"
                                 : "UP/DOWN: đổi số · ENTER: LƯU · EXIT: huỷ";
}

const ui_screen_t scr_edit = {
    .title = "", .page = -1, .refresh_ms = 1000,
    .draw_static = draw_static, .draw_values = draw_values, .key = key, .hint = hint,
};
