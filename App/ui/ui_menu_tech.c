/**
 * @file    ui_menu_tech.c
 * @brief   Menu kỹ thuật (ẩn): thông số bảo vệ, trễ, bù sai số cảm biến.
 *          Vào: giữ EXIT 3 s ở trang chính.
 *          UP/DOWN chọn · ENTER sửa · UP/DOWN đổi giá trị theo bước · ENTER xác nhận · EXIT huỷ/thoát
 *          Thoát menu có thay đổi → UI_CMD_SAVE_SETTINGS.
 */
#include "ui_internal.h"

#define ROW_Y0    30
#define ROW_H     LINE_H
#define X_VAL     200
#define ROWS      8

static uint8_t s_sel, s_top;
static bool    s_editing, s_changed;
static float   s_val;

static const ui_param_if_t *tp(void) { return g_ui.cfg->tech; }

static void draw_row(uint8_t idx, uint8_t row)
{
    int16_t y = (int16_t)(ROW_Y0 + row * ROW_H);
    ui_param_desc_t d;
    if (!tp() || idx >= tp()->count() || !tp()->desc(idx, &d)) {
        ILI9341_FillRect(0, y, TFT_WIDTH, ROW_H, UC_BG);
        return;
    }
    bool sel  = (idx == s_sel);
    bool edit = sel && s_editing;
    uint16_t bg = edit ? UC_CURSOR : (sel ? UC_SEL : UC_BG);
    uint16_t fg = edit ? C_BLACK : UC_VALUE;
    char num[12], val[24];
    Fmt_Float(num, sizeof(num), edit ? s_val : tp()->get(idx), d.dec);
    snprintf(val, sizeof(val), "%s %s", num, d.unit);
    ILI9341_FillRect(0, y, 8, ROW_H, bg);
    w_text(8, y, X_VAL - 8, d.name, fg, bg, TEXT_LEFT);
    w_text(X_VAL, y, TFT_WIDTH - 8 - X_VAL, val, fg, bg, TEXT_RIGHT);
    ILI9341_FillRect(TFT_WIDTH - 8, y, 8, ROW_H, bg);
}

static void enter(void)
{
    s_sel = s_top = 0;
    s_editing = s_changed = false;
}

static void draw_values(bool full)
{
    static uint8_t last_sel = 0xFF;
    static float   last_val;
    static bool    last_edit;
    if (!full && last_sel == s_sel && last_edit == s_editing && last_val == s_val) return;
    last_sel = s_sel; last_edit = s_editing; last_val = s_val;
    for (uint8_t r = 0; r < ROWS; r++) draw_row((uint8_t)(s_top + r), r);
}

static void move(int8_t dir)
{
    uint8_t n = tp() ? tp()->count() : 0;
    if (!n) return;
    s_sel = (uint8_t)((s_sel + n + dir) % n);
    if (s_sel < s_top) s_top = s_sel;
    if (s_sel >= s_top + ROWS) s_top = (uint8_t)(s_sel - ROWS + 1);
}

static void step(int8_t dir)
{
    ui_param_desc_t d;
    if (!tp()->desc(s_sel, &d)) return;
    s_val += dir * d.step;
    if (s_val < d.min) s_val = d.min;
    if (s_val > d.max) s_val = d.max;
}

static void key(ui_key_t k, ui_press_t p)
{
    if (!tp()) { ui_goto(SCR_MAIN); return; }
    if (p == UI_PRESS_LONG) return;

    if (!s_editing) {
        if (k == UI_KEY_UP)   move(-1);
        if (k == UI_KEY_DOWN) move(+1);
        if (k == UI_KEY_ENTER) { s_val = tp()->get(s_sel); s_editing = true; }
        if (k == UI_KEY_EXIT) {
            if (s_changed) {
                ui_cmd_t c = { .type = UI_CMD_SAVE_SETTINGS };
                UI_Message(ui_send(&c) ? "Đã lưu cài đặt" : "Lỗi lưu Flash!");
            }
            ui_goto(SCR_MAIN);
        }
    } else {
        if (k == UI_KEY_UP)   step(+1);
        if (k == UI_KEY_DOWN) step(-1);
        if (k == UI_KEY_ENTER) {
            if (s_val != tp()->get(s_sel)) { tp()->set(s_sel, s_val); s_changed = true; }
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

const ui_screen_t scr_tech = {
    .title = "KỸ THUẬT", .page = -1, .refresh_ms = 1000,
    .enter = enter, .draw_values = draw_values, .key = key, .hint = hint,
};
