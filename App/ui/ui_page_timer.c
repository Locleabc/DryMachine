/**
 * @file    ui_page_timer.c
 * @brief   Trang 3 – thời gian sấy (HH:MM, 00:00 = không giới hạn).
 */
#include "ui_internal.h"

#define Y_LABEL  44
#define Y_BIG    70
#define Y_NOTE   130
#define Y_REMAIN 168

static void draw_static(void)
{
    w_text_center(Y_LABEL, "Thoi gian say dat", 20, UC_LABEL, UC_BG, 2);
    w_text_center(Y_NOTE, "00:00 = khong gioi han", 24, UC_LABEL, UC_BG, 2);
}

static void draw_values(bool full)
{
    const ui_view_t *v = &g_ui.v;
    char a[24], b[12];
    (void)full;

    w_text_center(Y_BIG, w_fmt_hhmm(b, sizeof(b), v->dry_time_min), 5, UC_ACCENT, UC_BG, 6);

    bool running = (v->state == UI_ST_RUNNING || v->state == UI_ST_STARTING);
    if (running && v->dry_time_min) snprintf(a, sizeof(a), "Con lai %s", Fmt_Time(b, sizeof(b), w_remaining_s()));
    else a[0] = '\0';
    w_text_center(Y_REMAIN, a, 20, UC_VALUE, UC_BG, 2);
}

static void edit_done(const uint8_t *d, uint8_t n)
{
    ui_cmd_t c = { .type = UI_CMD_SET_DRY_TIME };
    if (n >= 4) {
        uint16_t hh = (uint16_t)(d[0] * 10 + d[1]);
        uint16_t mm = (uint16_t)(d[2] * 10 + d[3]);
        if (mm > 59) mm = 59;
        c.u.minutes = (uint16_t)(hh * 60 + mm);
        UI_Message(ui_send(&c) ? "DA LUU THOI GIAN" : "LOI LUU!");
    }
    ui_goto(SCR_TIMER);
}

static void key(ui_key_t k, ui_press_t p)
{
    if (k == UI_KEY_ENTER && p == UI_PRESS_SHORT) {
        char t[8];
        w_fmt_hhmm(t, sizeof(t), g_ui.v.dry_time_min);
        ui_edit_begin("THOI GIAN SAY", "Gio : Phut", t, edit_done, SCR_TIMER);
        return;
    }
    ui_page_nav(k, p);
}

static const char *hint(void)
{
    return "ENTER: chinh   EXIT: ve chinh";
}

const ui_screen_t scr_timer = {
    .title = "THOI GIAN SAY", .page = 2, .refresh_ms = 1000,
    .draw_static = draw_static, .draw_values = draw_values, .key = key, .hint = hint,
};
