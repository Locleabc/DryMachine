/**
 * @file    ui.c
 */
#include "ui.h"
#include "ili9341.h"
#include "util_fmt.h"
#include <stdio.h>
#include <string.h>

#define COL_BG        C_BLACK
#define COL_HEAD_BG   C_NAVY
#define COL_LABEL     C_GRAY
#define COL_VALUE     C_WHITE
#define COL_SEL_BG    C_YELLOW
#define COL_EDIT_BG   C_ORANGE

#define MENU_ROW_H    24
#define MENU_TOP      32
#define MENU_ROWS     8
#define MSG_SHOW_MS   2000
#define VALUE_REFRESH_MS 500

typedef enum { SCR_MAIN = 0, SCR_MENU, SCR_EDIT } screen_t;

static const ui_param_if_t *s_par;
static ui_cmd_cb_t s_cmd;

static struct {
    screen_t scr;
    bool     redraw, menu_dirty, changed;
    uint8_t  sel, top;
    float    edit_val;
    char     msg[32];
    bool     msg_new;
    uint32_t msg_tick, now, last_val;
} ui;

/* ---------------- tiện ích vẽ ---------------- */
static void draw_header(const char *title)
{
    ILI9341_FillRect(0, 0, TFT_WIDTH, 26, COL_HEAD_BG);
    ILI9341_DrawString(8, 5, title, C_WHITE, COL_HEAD_BG, 2);
}

/* In chuỗi, đệm khoảng trắng đủ width ký tự để xoá chữ cũ */
static void draw_field(int16_t x, int16_t y, const char *s, uint8_t width,
                       uint16_t fg, uint16_t bg, uint8_t scale)
{
    char buf[40];
    size_t n = strlen(s);
    if (width >= sizeof(buf)) width = sizeof(buf) - 1;
    if (n > width) n = width;
    memcpy(buf, s, n);
    memset(buf + n, ' ', width - n);
    buf[width] = '\0';
    ILI9341_DrawString(x, y, buf, fg, bg, scale);
}

static void fmt_value(char *out, size_t size, float v, uint8_t dec, const char *unit, bool ok)
{
    char num[12];
    if (ok) snprintf(out, size, "%s %s", Fmt_Float(num, sizeof(num), v, dec), unit);
    else    snprintf(out, size, "--.- %s", unit);
}

/* ---------------- Màn hình chính ---------------- */
static void main_draw_static(void)
{
    ILI9341_FillScreen(COL_BG);
    draw_header("MAY SAY TACH AM");
    ILI9341_DrawString(8,  40, "Nhiet do", COL_LABEL, COL_BG, 2);
    ILI9341_DrawString(8,  90, "Do am",    COL_LABEL, COL_BG, 2);
    ILI9341_DrawString(8, 140, "Ap suat",  COL_LABEL, COL_BG, 2);
    ILI9341_DrawHLine(0, 180, TFT_WIDTH, C_DARKGRAY);
}

static void draw_relay(int16_t x, const char *name, bool on)
{
    uint16_t bg = on ? C_GREEN : C_DARKGRAY;
    ILI9341_FillRect(x, 188, 64, 22, bg);
    ILI9341_DrawString((int16_t)(x + 20), 192, name, C_BLACK, bg, 2);
}

static void main_draw_values(const ui_view_t *v)
{
    char s[40], n[16];

    uint16_t hbg = (v->alarm == UI_ALARM_FAULT) ? C_RED : COL_HEAD_BG;
    ILI9341_FillRect(200, 0, 120, 26, hbg);
    ILI9341_DrawString(206, 5, v->state_text ? v->state_text : "", C_WHITE, hbg, 2);

    fmt_value(s, sizeof(s), v->temp, 1, "C", v->temp_ok);
    draw_field(120, 34, s, 8, COL_VALUE, COL_BG, 3);
    snprintf(s, sizeof(s), "/%s", Fmt_Float(n, sizeof(n), v->temp_set, 0));
    draw_field(272, 42, s, 4, COL_LABEL, COL_BG, 2);

    fmt_value(s, sizeof(s), v->hum, 1, "%", v->hum_ok);
    draw_field(120, 84, s, 8, v->hum_reached ? C_GREEN : COL_VALUE, COL_BG, 3);
    snprintf(s, sizeof(s), "/%s", Fmt_Float(n, sizeof(n), v->hum_set, 0));
    draw_field(272, 92, s, 4, COL_LABEL, COL_BG, 2);

    fmt_value(s, sizeof(s), v->press, 1, "bar", v->press_ok);
    draw_field(120, 134, s, 9, COL_VALUE, COL_BG, 3);

    draw_relay(8,   "MN", v->comp);
    draw_relay(80,  "QN", v->fan_cond);
    draw_relay(152, "QL", v->fan_evap);

    uint16_t fg = COL_VALUE;
    if (ui.msg[0] && (ui.now - ui.msg_tick) < MSG_SHOW_MS) {
        snprintf(s, sizeof(s), "%s", ui.msg);                          fg = C_CYAN;
    } else if (v->alarm != UI_ALARM_NONE && v->alarm_text) {
        snprintf(s, sizeof(s), "%s", v->alarm_text);
        fg = (v->alarm == UI_ALARM_FAULT) ? C_RED : C_YELLOW;
    } else if (v->running && v->comp_wait_s) {
        snprintf(s, sizeof(s), "Cho may nen %lus", (unsigned long)v->comp_wait_s); fg = C_YELLOW;
    } else if (v->running) {
        snprintf(s, sizeof(s), "Thoi gian %s", Fmt_Time(n, sizeof(n), v->run_s));
    } else {
        snprintf(s, sizeof(s), "Giu ENTER de chay");                   fg = COL_LABEL;
    }
    draw_field(8, 218, s, 25, fg, COL_BG, 2);
}

/* ---------------- Menu ---------------- */
static void menu_draw_row(uint8_t idx, uint8_t row)
{
    int16_t y = (int16_t)(MENU_TOP + row * MENU_ROW_H);
    ui_param_desc_t d;
    if (idx >= s_par->count() || !s_par->desc(idx, &d)) {
        ILI9341_FillRect(0, y, TFT_WIDTH, MENU_ROW_H, COL_BG);
        return;
    }
    bool sel  = (idx == ui.sel);
    bool edit = sel && ui.scr == SCR_EDIT;
    uint16_t bg = edit ? COL_EDIT_BG : (sel ? COL_SEL_BG : COL_BG);
    uint16_t fg = sel ? C_BLACK : COL_VALUE;

    char val[20], line[32];
    fmt_value(val, sizeof(val), edit ? ui.edit_val : s_par->get(idx), d.dec, d.unit, true);
    snprintf(line, sizeof(line), "%-14.14s%11s", d.name, val);
    ILI9341_FillRect(0, y, TFT_WIDTH, MENU_ROW_H, bg);
    ILI9341_DrawString(4, (int16_t)(y + 4), line, fg, bg, 2);
}

static void menu_draw_list(void)
{
    for (uint8_t r = 0; r < MENU_ROWS; r++) menu_draw_row((uint8_t)(ui.top + r), r);
}

static void menu_move(int8_t dir)
{
    uint8_t n = s_par->count();
    if (!n) return;
    ui.sel = (uint8_t)((ui.sel + n + dir) % n);
    if (ui.sel < ui.top) ui.top = ui.sel;
    if (ui.sel >= ui.top + MENU_ROWS) ui.top = (uint8_t)(ui.sel - MENU_ROWS + 1);
    ui.menu_dirty = true;
}

static void edit_step(int8_t dir)
{
    ui_param_desc_t d;
    if (!s_par->desc(ui.sel, &d)) return;
    ui.edit_val += dir * d.step;
    if (ui.edit_val < d.min) ui.edit_val = d.min;
    if (ui.edit_val > d.max) ui.edit_val = d.max;
    ui.menu_dirty = true;
}

static bool send_cmd(ui_cmd_t cmd)
{
    return s_cmd ? s_cmd(cmd) : false;
}

/* ---------------- API ---------------- */
void UI_Init(const ui_param_if_t *params, ui_cmd_cb_t on_cmd)
{
    memset(&ui, 0, sizeof(ui));
    s_par = params;
    s_cmd = on_cmd;
    ui.scr = SCR_MAIN;
    ui.redraw = true;
}

void UI_Message(const char *msg)
{
    snprintf(ui.msg, sizeof(ui.msg), "%s", msg);
    ui.msg_tick = ui.now;
    ui.msg_new  = true;
}

void UI_Key(ui_key_t key, ui_press_t press)
{
    bool is_short = (press == UI_PRESS_SHORT);
    bool step     = (press == UI_PRESS_SHORT || press == UI_PRESS_REPEAT);

    switch (ui.scr) {
    case SCR_MAIN:
        if (key == UI_KEY_ENTER && is_short)               { ui.scr = SCR_MENU; ui.redraw = true; }
        else if (key == UI_KEY_ENTER && press == UI_PRESS_LONG) send_cmd(UI_CMD_START_STOP);
        else if (key == UI_KEY_EXIT  && press == UI_PRESS_LONG) send_cmd(UI_CMD_RESET_FAULT);
        break;

    case SCR_MENU:
        if (key == UI_KEY_UP   && step) menu_move(-1);
        if (key == UI_KEY_DOWN && step) menu_move(+1);
        if (key == UI_KEY_ENTER && is_short) {
            ui.edit_val = s_par->get(ui.sel);
            ui.scr = SCR_EDIT; ui.menu_dirty = true;
        }
        if (key == UI_KEY_EXIT && is_short) {
            if (ui.changed) {
                UI_Message(send_cmd(UI_CMD_SAVE_SETTINGS) ? "DA LUU CAI DAT" : "LOI LUU FLASH!");
                ui.changed = false;
            }
            ui.scr = SCR_MAIN; ui.redraw = true;
        }
        break;

    case SCR_EDIT:
        if (key == UI_KEY_UP   && step) edit_step(+1);
        if (key == UI_KEY_DOWN && step) edit_step(-1);
        if (key == UI_KEY_ENTER && is_short) {
            if (ui.edit_val != s_par->get(ui.sel)) {
                s_par->set(ui.sel, ui.edit_val);
                ui.changed = true;
            }
            ui.scr = SCR_MENU; ui.menu_dirty = true;
        }
        if (key == UI_KEY_EXIT && is_short) { ui.scr = SCR_MENU; ui.menu_dirty = true; }
        break;
    }
}

void UI_Update(const ui_view_t *v, uint32_t now_ms)
{
    ui.now = now_ms;
    if (ui.scr == SCR_MAIN) {
        bool full = ui.redraw;
        if (full) { main_draw_static(); ui.redraw = false; }
        if (full || ui.msg_new || (now_ms - ui.last_val) >= VALUE_REFRESH_MS) {
            ui.msg_new = false;
            ui.last_val = now_ms;
            main_draw_values(v);
        }
    } else {
        if (ui.redraw) {
            ILI9341_FillScreen(COL_BG);
            draw_header("CAI DAT");
            ui.redraw = false;
            ui.menu_dirty = true;
        }
        if (ui.menu_dirty) { menu_draw_list(); ui.menu_dirty = false; }
    }
}
