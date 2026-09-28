/**
 * @file    ui_page_timer.c
 * @brief   Trang 3 – thời gian sấy (HH:MM, 00:00 = không giới hạn).
 */
#include "ui_internal.h"

#define Y_LABEL  38
#define Y_BIG    70
#define Y_NOTE   120
#define Y_REMAIN 156

static void draw_static(void)
{
    w_text(0, Y_LABEL, TFT_WIDTH, "Thời gian sấy đặt (giờ:phút)", UC_LABEL, UC_BG, TEXT_CENTER);
}

static void draw_values(bool full)
{
    const ui_view_t *v = &g_ui.v;
    char a[40], b[16];
    (void)full;

    w_num(0, Y_BIG, TFT_WIDTH, w_fmt_hhmm(b, sizeof(b), v->dry_time_min), v->manual ? UC_OFF : UC_ACCENT,
          UC_BG, TEXT_CENTER);

    if (v->manual) {
        w_text(0, Y_NOTE, TFT_WIDTH, "Chế độ Thủ công: không áp dụng", UC_WARN, UC_BG, TEXT_CENTER);
        w_text(0, Y_REMAIN, TFT_WIDTH, "(thời gian theo GĐ3, GĐ4 ở trang 4)", UC_LABEL, UC_BG, TEXT_CENTER);
        return;
    }
    w_text(0, Y_NOTE, TFT_WIDTH, "00:00 = không giới hạn", UC_LABEL, UC_BG, TEXT_CENTER);
    bool running = (v->state == UI_ST_RUNNING || v->state == UI_ST_STARTING);
    if (running && v->dry_time_min) snprintf(a, sizeof(a), "Còn lại %s", Fmt_Time(b, sizeof(b), w_remaining_s()));
    else a[0] = '\0';
    w_text(0, Y_REMAIN, TFT_WIDTH, a, UC_VALUE, UC_BG, TEXT_CENTER);
}

static void edit_done(const uint8_t *d, uint8_t n)
{
    ui_cmd_t c = { .type = UI_CMD_SET_DRY_TIME };
    if (n >= 4) {
        uint16_t hh = (uint16_t)(d[0] * 10 + d[1]);
        uint16_t mm = (uint16_t)(d[2] * 10 + d[3]);
        if (mm > 59) mm = 59;
        c.u.minutes = (uint16_t)(hh * 60 + mm);
        UI_Message(ui_send(&c) ? "Đã lưu thời gian sấy" : "Lỗi lưu!");
    }
    ui_goto(SCR_TIMER);
}

static void key(ui_key_t k, ui_press_t p)
{
    if (k == UI_KEY_ENTER && p == UI_PRESS_SHORT) {
        char t[8];
        w_fmt_hhmm(t, sizeof(t), g_ui.v.dry_time_min);
        ui_edit_begin("THỜI GIAN SẤY", "Giờ : Phút", t, edit_done, SCR_TIMER);
        return;
    }
    ui_page_nav(k, p);
}

static const char *hint(void)
{
    return "ENTER: chỉnh · EXIT: về chính";
}

const ui_screen_t scr_timer = {
    .title = "THỜI GIAN SẤY", .page = 2, .refresh_ms = 1000,
    .draw_static = draw_static, .draw_values = draw_values, .key = key, .hint = hint,
};
