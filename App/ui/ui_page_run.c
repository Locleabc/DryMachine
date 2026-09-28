/**
 * @file    ui_page_run.c
 * @brief   Trang 2 – chạy / dừng máy sấy, trạng thái chu trình.
 */
#include "ui_internal.h"

#define X_VAL     112
#define W_VAL     (TFT_WIDTH - 8 - X_VAL)
#define Y_STATE   30
#define Y_PHASE   53
#define Y_ELAPSED 76
#define Y_REMAIN  99
#define Y_COMP    122
#define Y_BTN     156
#define H_BTN     46

static void draw_static(void)
{
    w_label(8, Y_STATE,   "Trạng thái:");
    w_label(8, Y_PHASE,   "Giai đoạn:");
    w_label(8, Y_ELAPSED, "Đã sấy:");
    w_label(8, Y_REMAIN,  "Còn lại:");
    w_label(8, Y_COMP,    "Máy nén:");
}

static void draw_values(bool full)
{
    const ui_view_t *v = &g_ui.v;
    char a[48], b[16];

    snprintf(a, sizeof(a), "%s  (%s)", w_state_name(v->state), v->manual ? "Thủ công" : "Tự động");
    w_text(X_VAL, Y_STATE, W_VAL, a, w_state_color(v->state), UC_BG, TEXT_LEFT);

    bool running = (v->state == UI_ST_RUNNING || v->state == UI_ST_STARTING);
    const char *ph = (v->state == UI_ST_STARTING) ? "Quạt chạy trước máy nén"
                   : (v->phase_text && v->phase_text[0]) ? v->phase_text : "–";
    w_text(X_VAL, Y_PHASE, W_VAL, ph, UC_ACCENT, UC_BG, TEXT_LEFT);

    w_text(X_VAL, Y_ELAPSED, W_VAL, running ? Fmt_Time(b, sizeof(b), v->run_s) : "--:--:--", UC_VALUE, UC_BG, TEXT_LEFT);

    uint32_t rem = w_remaining_s();
    if (v->manual)                   snprintf(a, sizeof(a), "%s", rem ? Fmt_Time(b, sizeof(b), rem) : "–");
    else if (v->dry_time_min == 0)   snprintf(a, sizeof(a), "không giới hạn");
    else if (running)                Fmt_Time(a, sizeof(a), rem);
    else                             snprintf(a, sizeof(a), "%s (đặt)", w_fmt_hhmm(b, sizeof(b), v->dry_time_min));
    w_text(X_VAL, Y_REMAIN, W_VAL, a, UC_VALUE, UC_BG, TEXT_LEFT);

    if (v->comp)                        snprintf(a, sizeof(a), "Đang chạy");
    else if (running && v->comp_wait_s) snprintf(a, sizeof(a), "Chờ bật lại %lus", (unsigned long)v->comp_wait_s);
    else                                snprintf(a, sizeof(a), "Tắt");
    w_text(X_VAL, Y_COMP, W_VAL, a, v->comp ? UC_OK : UC_VALUE, UC_BG, TEXT_LEFT);

    /* nút hành động – chỉ vẽ lại khi trạng thái đổi */
    static ui_state_t last = (ui_state_t)0xFF;
    if (!full && last == v->state) return;
    last = v->state;
    const char *txt; uint16_t bg, fg = C_BLACK;
    switch (v->state) {
    case UI_ST_IDLE:     txt = "ENTER: BẮT ĐẦU SẤY";     bg = UC_OK;  break;
    case UI_ST_STARTING:
    case UI_ST_RUNNING:  txt = "ENTER: DỪNG SẤY";        bg = UC_ERR; fg = C_WHITE;  break;
    case UI_ST_STOPPING: txt = "Đang tắt quạt…";         bg = UC_OFF; fg = UC_LABEL; break;
    default:             txt = "Đang lỗi – xem trang 5"; bg = UC_OFF; fg = UC_ERR;   break;
    }
    int16_t pad = (H_BTN - LINE_H) / 2;
    ILI9341_FillRect(8, Y_BTN, TFT_WIDTH - 16, pad, bg);
    Text_Box(8, Y_BTN + pad, TFT_WIDTH - 16, txt, F_TXT, fg, bg, TEXT_CENTER);
    ILI9341_FillRect(8, (int16_t)(Y_BTN + pad + LINE_H), TFT_WIDTH - 16, (int16_t)(H_BTN - pad - LINE_H), bg);
}

static void key(ui_key_t k, ui_press_t p)
{
    if (k == UI_KEY_ENTER && p == UI_PRESS_SHORT) {
        ui_cmd_t c = { .type = UI_CMD_START_STOP };
        ui_send(&c);
        return;
    }
    ui_page_nav(k, p);
}

static const char *hint(void)
{
    return "ENTER: chạy/dừng · EXIT: thoát";
}

const ui_screen_t scr_run = {
    .title = "CHẠY / DỪNG", .page = 1, .refresh_ms = 500,
    .draw_static = draw_static, .draw_values = draw_values, .key = key, .hint = hint,
};
