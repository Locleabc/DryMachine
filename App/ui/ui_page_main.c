/**
 * @file    ui_page_main.c
 * @brief   Trang 1 – nhiệt độ, độ ẩm thực tế + điểm đặt, chế độ sấy, trạng thái, relay.
 *
 *   ┌ MÁY SẤY ──────────────── 14:05:32 ┐
 *   │ Nhiệt độ            Độ ẩm          │
 *   │ 54.6°C              21.4%          │  (số lớn)
 *   │ Đặt 55°C            Đặt 15%        │
 *   │ ─────────────────────────────────  │
 *   │ Chế độ: Trái cây                   │
 *   │ Đang sấy            Đã sấy 01:23:45│
 *   │ [MN][QN][QL]        Còn    04:36:15│
 *   │ Lỗi / cảnh báo                     │
 *   └ UP/DOWN: trang · Giữ ENTER … ■□□□ ┘
 */
#include "ui_internal.h"

#define COL_L    8
#define COL_R    168
#define COL_W    144
#define Y_LABEL  29
#define Y_BIG    51
#define Y_SET    88
#define Y_LINE   114
#define Y_MODE   118
#define Y_STATE  142
#define Y_RELAY  166
#define Y_ALARM  192
#define X_TIME   180          /* cột thời gian bên phải */
#define W_TIME   (TFT_WIDTH - 8 - X_TIME)

static void draw_static(void)
{
    w_label(COL_L, Y_LABEL, "Nhiệt độ (°C)");
    w_label(COL_R, Y_LABEL, "Độ ẩm (%RH)");
    w_hline(Y_LINE);
    w_label(COL_L, Y_MODE, "Chế độ:");
}

static void draw_values(bool full)
{
    const ui_view_t *v = &g_ui.v;
    char a[40], b[16];
    (void)full;

    /* số đo lớn */
    w_fmt_value(b, sizeof(b), v->temp_ok, v->temp);
    snprintf(a, sizeof(a), "%s°", b);
    w_num(COL_L, Y_BIG, COL_W, a, v->temp_ok ? UC_TEMP : UC_LABEL, UC_BG, TEXT_LEFT);
    w_fmt_value(b, sizeof(b), v->hum_ok, v->hum);
    snprintf(a, sizeof(a), "%s%%", b);
    w_num(COL_R, Y_BIG, COL_W, a, v->hum_ok ? (v->hum_reached ? UC_OK : UC_HUM) : UC_LABEL, UC_BG, TEXT_LEFT);

    /* điểm đặt */
    snprintf(a, sizeof(a), "Đặt %s°C", Fmt_Float(b, sizeof(b), v->temp_set, 0));
    w_text(COL_L, Y_SET, COL_W, a, UC_VALUE, UC_BG, TEXT_LEFT);
    snprintf(a, sizeof(a), "Đặt %s%%", Fmt_Float(b, sizeof(b), v->hum_set, 0));
    w_text(COL_R, Y_SET, COL_W, a, UC_VALUE, UC_BG, TEXT_LEFT);

    /* chế độ */
    const char *name = (v->preset < g_ui.cfg->preset_count) ? g_ui.cfg->preset_names[v->preset] : "?";
    w_text(COL_L + 64, Y_MODE, TFT_WIDTH - 8 - (COL_L + 64), name, UC_ACCENT, UC_BG, TEXT_LEFT);

    /* trạng thái / giai đoạn + thời gian đã sấy */
    bool running = (v->state == UI_ST_RUNNING || v->state == UI_ST_STARTING);
    const char *st = (v->state == UI_ST_RUNNING && v->phase_text && v->phase_text[0]) ? v->phase_text
                                                                                      : w_state_name(v->state);
    w_text(COL_L, Y_STATE, X_TIME - COL_L, st, w_state_color(v->state), UC_BG, TEXT_LEFT);
    w_text(X_TIME, Y_STATE, W_TIME, running ? Fmt_Time(b, sizeof(b), v->run_s) : "", UC_VALUE, UC_BG, TEXT_RIGHT);

    /* relay: máy nén, quạt dàn nóng (cấp), quạt dàn lạnh + thời gian còn lại / chờ máy nén */
    w_badge(COL_L,       Y_RELAY, 36, "MN", v->comp);
    w_badge(COL_L + 40,  Y_RELAY, 44, w_fan_badge(b, sizeof(b), v->fan_level), v->fan_level > 0);
    w_badge(COL_L + 88,  Y_RELAY, 36, "QL", v->fan_evap);
    uint32_t rem = w_remaining_s();
    if (running && v->comp_wait_s) snprintf(a, sizeof(a), "Chờ MN %lus", (unsigned long)v->comp_wait_s);
    else if (running && rem)        snprintf(a, sizeof(a), "Còn %s", Fmt_Time(b, sizeof(b), rem));
    else                            a[0] = '\0';
    w_text(COL_L + 128, Y_RELAY, TFT_WIDTH - 8 - (COL_L + 128), a, UC_LABEL, UC_BG, TEXT_RIGHT);

    /* lỗi / cảnh báo */
    if (v->fault_text) {
        snprintf(a, sizeof(a), "LỖI: %s", v->fault_text);
        w_text(COL_L, Y_ALARM, TFT_WIDTH - 16, a, UC_ERR, UC_BG, TEXT_LEFT);
    } else if (v->warn_text) {
        w_text(COL_L, Y_ALARM, TFT_WIDTH - 16, v->warn_text, UC_WARN, UC_BG, TEXT_LEFT);
    } else {
        w_text(COL_L, Y_ALARM, TFT_WIDTH - 16, "", UC_BG, UC_BG, TEXT_LEFT);
    }
}

static void key(ui_key_t k, ui_press_t p)
{
    if (k == UI_KEY_EXIT && p == UI_PRESS_LONG) { ui_list_open("KỸ THUẬT", g_ui.cfg->tech, SCR_MAIN); return; }
    ui_page_nav(k, p);
}

static const char *hint(void)
{
    return "UP/DN: trang · Giữ ENTER: chế độ";
}

const ui_screen_t scr_main = {
    .title = "MÁY SẤY", .page = 0, .refresh_ms = 500,
    .draw_static = draw_static, .draw_values = draw_values, .key = key, .hint = hint,
};
