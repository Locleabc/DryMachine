/**
 * @file    ui_list.c
 * @brief   Danh sách thông số dùng chung (menu kỹ thuật, cài đặt quạt / chu trình).
 *          Mở bằng ui_list_open(tiêu đề, danh sách, màn hình quay về).
 *          UP/DOWN chọn · ENTER sửa · UP/DOWN đổi giá trị theo bước · ENTER xác nhận · EXIT huỷ/thoát
 *          Thoát có thay đổi → UI_CMD_SAVE_SETTINGS.
 */
#include "ui_internal.h"

#define ROW_Y0    30
#define ROW_H     LINE_H
#define ROWS      8
#define X_VAL     196

static const char          *s_title;
static const ui_param_if_t *s_list;
static ui_scr_t             s_back;
static uint8_t s_sel, s_top;
static bool    s_editing, s_changed;
static float   s_val;

void ui_list_open(const char *title, const ui_param_if_t *list, ui_scr_t back)
{
    s_title = title;
    s_list  = list;
    s_back  = back;
    s_sel = s_top = 0;
    s_editing = s_changed = false;
    ui_goto(SCR_LIST);
}

static void fmt_val(char *out, size_t n, const ui_param_desc_t *d, float v)
{
    if (d->choices) {
        int i = (int)(v + 0.5f);
        if (i < (int)(d->min + 0.5f)) i = (int)(d->min + 0.5f);
        snprintf(out, n, "%s", d->choices[i]);
        return;
    }
    char num[12];
    Fmt_Float(num, sizeof(num), v, d->dec);
    if (d->unit && d->unit[0]) snprintf(out, n, "%s %s", num, d->unit);
    else                       snprintf(out, n, "%s", num);
}

static void draw_row(uint8_t idx, uint8_t row)
{
    int16_t y = (int16_t)(ROW_Y0 + row * ROW_H);
    ui_param_desc_t d;
    if (!s_list || idx >= s_list->count() || !s_list->desc(idx, &d)) {
        ILI9341_FillRect(0, y, TFT_WIDTH, ROW_H, UC_BG);
        return;
    }
    bool sel  = (idx == s_sel);
    bool edit = sel && s_editing;
    uint16_t bg = edit ? UC_CURSOR : (sel ? UC_SEL : UC_BG);
    uint16_t fg = edit ? C_BLACK : UC_VALUE;
    char val[32];
    fmt_val(val, sizeof(val), &d, edit ? s_val : s_list->get(idx));
    ILI9341_FillRect(0, y, 8, ROW_H, bg);
    w_text(8, y, X_VAL - 8, d.name, fg, bg, TEXT_LEFT);
    w_text(X_VAL, y, TFT_WIDTH - 8 - X_VAL, val, edit ? C_BLACK : (d.choices ? UC_ACCENT : UC_VALUE), bg, TEXT_RIGHT);
    ILI9341_FillRect(TFT_WIDTH - 8, y, 8, ROW_H, bg);
}

static void draw_static(void)
{
    Text_Box(8, 2, 210, s_title ? s_title : "", F_TXT, C_WHITE, UC_HEAD, TEXT_LEFT);
}

static void draw_values(bool full)
{
    static uint8_t last_sel = 0xFF, last_top = 0xFF;
    static float   last_val;
    static bool    last_edit;
    if (!full && last_sel == s_sel && last_top == s_top && last_edit == s_editing && last_val == s_val) return;
    last_sel = s_sel; last_top = s_top; last_edit = s_editing; last_val = s_val;
    for (uint8_t r = 0; r < ROWS; r++) draw_row((uint8_t)(s_top + r), r);
}

static void move(int8_t dir)
{
    uint8_t n = s_list ? s_list->count() : 0;
    if (!n) return;
    s_sel = (uint8_t)((s_sel + n + dir) % n);
    if (s_sel < s_top) s_top = s_sel;
    if (s_sel >= s_top + ROWS) s_top = (uint8_t)(s_sel - ROWS + 1);
}

static void step(int8_t dir)
{
    ui_param_desc_t d;
    if (!s_list->desc(s_sel, &d)) return;
    s_val += dir * d.step;
    if (s_val < d.min) s_val = d.min;
    if (s_val > d.max) s_val = d.max;
}

static void key(ui_key_t k, ui_press_t p)
{
    if (!s_list) { ui_goto(s_back); return; }
    if (p == UI_PRESS_LONG) return;

    if (!s_editing) {
        if (k == UI_KEY_UP)   move(-1);
        if (k == UI_KEY_DOWN) move(+1);
        if (k == UI_KEY_ENTER) { s_val = s_list->get(s_sel); s_editing = true; }
        if (k == UI_KEY_EXIT) {
            if (s_changed) {
                ui_cmd_t c = { .type = UI_CMD_SAVE_SETTINGS };
                UI_Message(ui_send(&c) ? "Đã lưu cài đặt" : "Lỗi lưu Flash!");
            }
            ui_goto(s_back);
        }
    } else {
        if (k == UI_KEY_UP)   step(+1);
        if (k == UI_KEY_DOWN) step(-1);
        if (k == UI_KEY_ENTER) {
            if (s_val != s_list->get(s_sel)) { s_list->set(s_sel, s_val); s_changed = true; }
            s_editing = false;
        }
        if (k == UI_KEY_EXIT) s_editing = false;
    }
}

static const char *hint(void)
{
    return s_editing ? "UP/DOWN: đổi · ENTER: OK · EXIT: huỷ"
                     : "ENTER: sửa · EXIT: lưu & thoát";
}

const ui_screen_t scr_list = {
    .title = "", .page = -1, .refresh_ms = 1000,
    .draw_static = draw_static, .draw_values = draw_values, .key = key, .hint = hint,
};
