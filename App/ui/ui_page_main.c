/**
 * @file    ui_page_main.c
 * @brief   Trang 1 – nhiệt độ, độ ẩm thực tế + điểm đặt, chế độ sấy, trạng thái, relay.
 *
 *   ┌ MAY SAY ─────────────── 14:05:32 ┐
 *   │ NHIET DO          DO AM          │
 *   │ 55.3 C            18.2 %         │  (số lớn)
 *   │ Dat 55 C          Dat 15 %       │
 *   │ ──────────────────────────────── │
 *   │ Che do: Trai cay                 │
 *   │ DANG SAY           Da say 01:23  │
 *   │ [MN][QN][QL]       Con   00:36   │
 *   │ LOI / CANH BAO                   │
 *   └ Giu ENTER 3s: che do ... ■□□□ ───┘
 */
#include "ui_internal.h"

#define COL_L    8
#define COL_R    168
#define Y_LABEL  36
#define Y_BIG    54
#define Y_SET    100
#define Y_LINE   124
#define Y_MODE   132
#define Y_STATE  156
#define Y_RELAY  178
#define Y_ALARM  202
#define COL_TIME 140     /* cột thời gian bên phải, 15 ký tự x 12 px = 180 px */

static void draw_static(void)
{
    ILI9341_DrawString(COL_L, Y_LABEL, "NHIET DO", UC_LABEL, UC_BG, 2);
    ILI9341_DrawString(COL_R, Y_LABEL, "DO AM",    UC_LABEL, UC_BG, 2);
    w_hline(Y_LINE);
    ILI9341_DrawString(COL_L, Y_MODE, "Che do:", UC_LABEL, UC_BG, 2);
}

static void big_value(int16_t x, bool ok, float v, const char *unit, uint16_t color)
{
    char s[8];
    w_fmt_value(s, sizeof(s), ok, v);
    w_text(x, Y_BIG, s, 4, ok ? color : UC_LABEL, UC_BG, 5);            /* 4 ký tự x 30 px */
    ILI9341_DrawString((int16_t)(x + 124), (int16_t)(Y_BIG + 4), unit, color, UC_BG, 2);
}

static void draw_values(bool full)
{
    const ui_view_t *v = &g_ui.v;
    char a[24], b[12];
    (void)full;

    /* số đo lớn + điểm đặt */
    big_value(COL_L, v->temp_ok, v->temp, "C", UC_TEMP);
    big_value(COL_R, v->hum_ok,  v->hum,  "%", UC_HUM);
    snprintf(a, sizeof(a), "Dat %s C", Fmt_Float(b, sizeof(b), v->temp_set, 0));
    w_text(COL_L, Y_SET, a, 11, UC_VALUE, UC_BG, 2);
    snprintf(a, sizeof(a), "Dat %s %%", Fmt_Float(b, sizeof(b), v->hum_set, 0));
    w_text(COL_R, Y_SET, a, 11, UC_VALUE, UC_BG, 2);

    /* chế độ */
    const char *name = (v->preset < g_ui.cfg->preset_count) ? g_ui.cfg->preset_names[v->preset] : "?";
    w_text(COL_L + 96, Y_MODE, name, 17, UC_ACCENT, UC_BG, 2);

    /* trạng thái + thời gian */
    w_text(COL_L, Y_STATE, w_state_name(v->state), 10, w_state_color(v->state), UC_BG, 2);
    bool running = (v->state == UI_ST_RUNNING || v->state == UI_ST_STARTING);
    if (running) snprintf(a, sizeof(a), "Da say %s", Fmt_Time(b, sizeof(b), v->run_s));
    else         a[0] = '\0';
    w_text(COL_TIME, Y_STATE, a, 15, UC_VALUE, UC_BG, 2);

    /* relay + thời gian còn lại */
    w_badge(COL_L,       Y_RELAY, 38, "MN", v->comp);
    w_badge(COL_L + 42,  Y_RELAY, 38, "QN", v->fan_cond);
    w_badge(COL_L + 84,  Y_RELAY, 38, "QL", v->fan_evap);
    uint32_t rem = w_remaining_s();
    if (running && rem) snprintf(a, sizeof(a), "Con    %s", Fmt_Time(b, sizeof(b), rem));
    else if (running && v->comp_wait_s) snprintf(a, sizeof(a), "Cho MN %lus", (unsigned long)v->comp_wait_s);
    else a[0] = '\0';
    w_text(COL_TIME, Y_RELAY + 2, a, 15, UC_LABEL, UC_BG, 2);

    /* lỗi / cảnh báo */
    if (v->fault_text)     { snprintf(a, sizeof(a), "LOI: %s", v->fault_text); w_text(COL_L, Y_ALARM, a, 25, UC_ERR,  UC_BG, 2); }
    else if (v->warn_text) {                                                      w_text(COL_L, Y_ALARM, v->warn_text, 25, UC_WARN, UC_BG, 2); }
    else                   {                                                      w_text(COL_L, Y_ALARM, "", 25, UC_BG, UC_BG, 2); }
}

static void key(ui_key_t k, ui_press_t p)
{
    if (k == UI_KEY_EXIT && p == UI_PRESS_LONG) { ui_goto(SCR_TECH); return; }
    ui_page_nav(k, p);
}

static const char *hint(void)
{
    return "UP/DN: trang   Giu ENTER 3s: che do";
}

const ui_screen_t scr_main = {
    .title = "MAY SAY", .page = 0, .refresh_ms = 500,
    .draw_static = draw_static, .draw_values = draw_values, .key = key, .hint = hint,
};
