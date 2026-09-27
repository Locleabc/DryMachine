/**
 * @file    ui_menu.c
 */
#include "ui_menu.h"
#include "drv_ili9341.h"
#include "settings.h"
#include "util_fmt.h"
#include "app_config.h"
#include <stdio.h>
#include <string.h>

/* ---- Bố cục ---- */
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

typedef enum { SCR_MAIN = 0, SCR_MENU, SCR_EDIT } screen_t;

static struct {
    screen_t scr;
    bool     redraw;          /* vẽ lại toàn màn hình */
    bool     menu_dirty;      /* cần vẽ lại danh sách */
    uint8_t  sel, top;        /* mục chọn, mục đầu tiên hiển thị */
    float    edit_val;
    bool     settings_changed;
    char     msg[32];
    uint32_t msg_tick;
    bool     msg_new;         /* có thông báo mới → vẽ ngay */
} ui;

/* ---------------- tiện ích vẽ ---------------- */
static void draw_header(const char *title)
{
    ILI9341_FillRect(0, 0, TFT_WIDTH, 26, COL_HEAD_BG);
    ILI9341_DrawString(8, 5, title, C_WHITE, COL_HEAD_BG, 2);
}

/* Chuỗi căn trái, đệm khoảng trắng đến width ký tự để xoá chữ cũ */
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

static void format_param(char *out, size_t size, uint8_t idx, float v)
{
    const param_desc_t *d = Settings_ParamDesc(idx);
    char num[12];
    Fmt_Float(num, sizeof(num), v, d->dec);
    snprintf(out, size, "%s %s", num, d->unit);
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
    ILI9341_DrawString((int16_t)(x + 8), 192, name, C_BLACK, bg, 2);
}

static void main_draw_values(const app_meas_t *m, const ctrl_status_t *st)
{
    char v[16], sp[16], line[40];

    /* Trạng thái góc phải header */
    uint16_t hbg = (st->state == CTRL_FAULT) ? C_RED : COL_HEAD_BG;
    ILI9341_FillRect(200, 0, 120, 26, hbg);
    ILI9341_DrawString(206, 5, Ctrl_StateName(st->state), C_WHITE, hbg, 2);

    /* Nhiệt độ */
    if (m->temp.ok) snprintf(line, sizeof(line), "%s C", Fmt_Float(v, sizeof(v), m->temp.temp_c, 1));
    else            strcpy(line, "---.- C");
    draw_field(120, 34, line, 8, COL_VALUE, COL_BG, 3);
    snprintf(line, sizeof(line), "/%s", Fmt_Float(sp, sizeof(sp), g_settings.temp_set, 0));
    draw_field(272, 42, line, 4, COL_LABEL, COL_BG, 2);

    /* Độ ẩm */
    if (m->hum.ok) snprintf(line, sizeof(line), "%s %%", Fmt_Float(v, sizeof(v), m->hum.rh, 1));
    else           strcpy(line, "--.- %");
    draw_field(120, 84, line, 8, st->target_reached ? C_GREEN : COL_VALUE, COL_BG, 3);
    snprintf(line, sizeof(line), "/%s", Fmt_Float(sp, sizeof(sp), g_settings.hum_set, 0));
    draw_field(272, 92, line, 4, COL_LABEL, COL_BG, 2);

    /* Áp suất */
    if (m->press.ok) snprintf(line, sizeof(line), "%s bar", Fmt_Float(v, sizeof(v), m->press.bar, 1));
    else             strcpy(line, "--.- bar");
    draw_field(120, 134, line, 9, COL_VALUE, COL_BG, 3);

    /* Relay */
    draw_relay(8,   "MN", st->comp);
    draw_relay(80,  "QN", st->fan_cond);
    draw_relay(152, "QL", st->fan_evap);

    /* Dòng trạng thái */
    uint16_t fg = COL_VALUE;
    if (ui.msg[0] && (HAL_GetTick() - ui.msg_tick) < MSG_SHOW_MS) {
        snprintf(line, sizeof(line), "%s", ui.msg);
        fg = C_CYAN;
    } else if (st->faults) {
        snprintf(line, sizeof(line), "%s", Ctrl_FaultText(st->faults));
        fg = C_RED;
    } else if (st->warnings & WARN_HUM_SENSOR) {
        snprintf(line, sizeof(line), "CANH BAO: MAT CB AM");
        fg = C_YELLOW;
    } else if (st->comp_demand && st->comp_wait_s && st->state == CTRL_RUNNING) {
        snprintf(line, sizeof(line), "Cho may nen %lus", (unsigned long)st->comp_wait_s);
        fg = C_YELLOW;
    } else if (st->state == CTRL_RUNNING || st->state == CTRL_STARTING) {
        snprintf(line, sizeof(line), "Thoi gian %s", Fmt_Time(v, sizeof(v), st->run_s));
    } else {
        snprintf(line, sizeof(line), "Giu ENTER de chay");
        fg = COL_LABEL;
    }
    draw_field(8, 218, line, 25, fg, COL_BG, 2);
}

/* ---------------- Menu ---------------- */
static void menu_draw_row(uint8_t idx, uint8_t row)
{
    char val[20], line[30];
    int16_t y = (int16_t)(MENU_TOP + row * MENU_ROW_H);
    bool sel  = (idx == ui.sel);
    bool edit = sel && ui.scr == SCR_EDIT;
    uint16_t bg = edit ? COL_EDIT_BG : (sel ? COL_SEL_BG : COL_BG);
    uint16_t fg = sel ? C_BLACK : COL_VALUE;

    if (idx >= Settings_ParamCount()) {
        ILI9341_FillRect(0, y, TFT_WIDTH, MENU_ROW_H, COL_BG);
        return;
    }
    const param_desc_t *d = Settings_ParamDesc(idx);
    format_param(val, sizeof(val), idx, edit ? ui.edit_val : Settings_GetParam(idx));
    snprintf(line, sizeof(line), "%-14s%11s", d->name, val);
    ILI9341_FillRect(0, y, TFT_WIDTH, MENU_ROW_H, bg);
    ILI9341_DrawString(4, (int16_t)(y + 4), line, fg, bg, 2);
}

static void menu_draw_list(void)
{
    for (uint8_t r = 0; r < MENU_ROWS; r++) menu_draw_row((uint8_t)(ui.top + r), r);
}

static void menu_draw_static(void)
{
    ILI9341_FillScreen(COL_BG);
    draw_header("CAI DAT");
    menu_draw_list();
}

static void menu_move(int8_t dir)
{
    uint8_t n = Settings_ParamCount();
    ui.sel = (uint8_t)((ui.sel + n + dir) % n);
    if (ui.sel < ui.top) ui.top = ui.sel;
    if (ui.sel >= ui.top + MENU_ROWS) ui.top = (uint8_t)(ui.sel - MENU_ROWS + 1);
    ui.menu_dirty = true;
}

static void edit_step(int8_t dir)
{
    const param_desc_t *d = Settings_ParamDesc(ui.sel);
    ui.edit_val += dir * d->step;
    if (ui.edit_val < d->min) ui.edit_val = d->min;
    if (ui.edit_val > d->max) ui.edit_val = d->max;
    ui.menu_dirty = true;
}

/* ---------------- API ---------------- */
void UI_Init(void)
{
    memset(&ui, 0, sizeof(ui));
    ui.scr = SCR_MAIN;
    ui.redraw = true;
}

void UI_Message(const char *msg)
{
    snprintf(ui.msg, sizeof(ui.msg), "%s", msg);
    ui.msg_tick = HAL_GetTick();
    ui.msg_new  = true;
}

void UI_HandleButton(const btn_event_t *e)
{
    switch (ui.scr) {
    case SCR_MAIN:
        if (e->id == BTN_ENTER && e->type == BTN_EVT_CLICK) {
            ui.scr = SCR_MENU; ui.redraw = true;
        } else if (e->id == BTN_ENTER && e->type == BTN_EVT_LONG) {
            ctrl_status_t st;
            Ctrl_GetStatus(&st);
            if (st.state == CTRL_IDLE) { Ctrl_Start(); UI_Message("BAT DAU SAY"); }
            else if (st.state == CTRL_STARTING || st.state == CTRL_RUNNING) { Ctrl_Stop(); UI_Message("DUNG SAY"); }
        } else if (e->id == BTN_EXIT && e->type == BTN_EVT_LONG) {
            Ctrl_ResetFault();
            UI_Message("DA RESET LOI");
        }
        break;

    case SCR_MENU:
        if (e->id == BTN_UP)   menu_move(-1);
        if (e->id == BTN_DOWN) menu_move(+1);
        if (e->id == BTN_ENTER && e->type == BTN_EVT_CLICK) {
            ui.edit_val = Settings_GetParam(ui.sel);
            ui.scr = SCR_EDIT; ui.menu_dirty = true;
        }
        if (e->id == BTN_EXIT && e->type == BTN_EVT_CLICK) {
            if (ui.settings_changed) {
                UI_Message(Settings_Save() ? "DA LUU CAI DAT" : "LOI LUU FLASH!");
                ui.settings_changed = false;
            }
            ui.scr = SCR_MAIN; ui.redraw = true;
        }
        break;

    case SCR_EDIT:
        if (e->id == BTN_UP)   edit_step(+1);
        if (e->id == BTN_DOWN) edit_step(-1);
        if (e->id == BTN_ENTER && e->type == BTN_EVT_CLICK) {
            if (ui.edit_val != Settings_GetParam(ui.sel)) {
                Settings_SetParam(ui.sel, ui.edit_val);
                ui.settings_changed = true;
            }
            ui.scr = SCR_MENU; ui.menu_dirty = true;
        }
        if (e->id == BTN_EXIT && e->type == BTN_EVT_CLICK) {
            ui.scr = SCR_MENU; ui.menu_dirty = true;   /* huỷ */
        }
        break;
    }
}

void UI_Update(const app_meas_t *m, const ctrl_status_t *st)
{
    static uint32_t last_val;
    uint32_t now = HAL_GetTick();

    if (ui.scr == SCR_MAIN) {
        bool full = ui.redraw;
        if (full) { main_draw_static(); ui.redraw = false; }
        /* Giá trị chỉ vẽ lại mỗi 500 ms để giảm tải SPI */
        if (full || ui.msg_new || (now - last_val) >= 500) {
            ui.msg_new = false;
            last_val = now;
            main_draw_values(m, st);
        }
    } else {
        if (ui.redraw) { menu_draw_static(); ui.redraw = false; ui.menu_dirty = false; }
        if (ui.menu_dirty) { menu_draw_list(); ui.menu_dirty = false; }
    }
}
