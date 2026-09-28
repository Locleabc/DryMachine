/**
 * @file    ui_page_outputs.c
 * @brief   Trang 6 – trạng thái các đầu ra relay (ON / OFF).
 *
 *  Tên + chân lấy từ cfg->output_names / output_pins (app.c khai báo, UI không biết phần cứng).
 *  Mỗi đầu ra có 2 bit trong view:
 *    out_cmd   – bộ điều khiển đang yêu cầu BẬT
 *    out_relay – relay thật đang đóng
 *  Giả lập với "Relay thật" = Tắt: yêu cầu BẬT nhưng relay không đóng → hiện "ON*".
 *  Khi giả lập: ENTER đảo "Relay thật" (đo điện áp chân ra thực tế).
 */
#include "ui_internal.h"

#define Y0       29
#define X_NAME   8
#define X_PIN    160
#define X_PILL   248
#define W_PILL   64
#define Y_NOTE   (Y0 + UI_OUT_MAX * LINE_H + 2)

static void draw_static(void)
{
    const ui_config_t *c = g_ui.cfg;
    for (uint8_t i = 0; i < c->output_count && i < UI_OUT_MAX; i++) {
        int16_t y = (int16_t)(Y0 + i * LINE_H);
        w_text(X_NAME, y, X_PIN - X_NAME - 4, c->output_names[i], UC_VALUE, UC_BG, TEXT_LEFT);
        if (c->output_pins) w_text(X_PIN, y, X_PILL - X_PIN - 6, c->output_pins[i], UC_LABEL, UC_BG, TEXT_LEFT);
    }
}

static void draw_values(bool full)
{
    const ui_view_t *v = &g_ui.v;
    static uint8_t last_cmd = 0xFF, last_rly = 0xFF;
    if (!full && last_cmd == v->out_cmd && last_rly == v->out_relay) return;
    last_cmd = v->out_cmd;
    last_rly = v->out_relay;

    bool mismatch = false;
    for (uint8_t i = 0; i < g_ui.cfg->output_count && i < UI_OUT_MAX; i++) {
        bool cmd = (v->out_cmd >> i) & 1u;
        bool rly = (v->out_relay >> i) & 1u;
        int16_t y = (int16_t)(Y0 + i * LINE_H);
        if (cmd && !rly) {                                  /* yêu cầu bật nhưng relay không đóng */
            Text_Box(X_PILL, y, W_PILL, "ON*", F_TXT, C_BLACK, UC_WARN, TEXT_CENTER);
            mismatch = true;
        } else {
            w_badge(X_PILL, y, W_PILL, rly ? "ON" : "OFF", rly);
        }
    }
    if (mismatch) w_text(X_NAME, Y_NOTE, TFT_WIDTH - 16, "* giả lập: relay không đóng", UC_WARN, UC_BG, TEXT_LEFT);
    else          w_text(X_NAME, Y_NOTE, TFT_WIDTH - 16, "Chân ra: ON = 0 V · OFF = 3.3 V", UC_LABEL, UC_BG, TEXT_LEFT);
}

static void key(ui_key_t k, ui_press_t p)
{
    if (k == UI_KEY_ENTER && p == UI_PRESS_SHORT && g_ui.sim_on) {
        ui_cmd_t c = { .type = UI_CMD_SIM_RELAY };
        ui_send(&c);
        g_ui.dirty = true;
        return;
    }
    ui_page_nav(k, p);
}

static const char *hint(void)
{
    return g_ui.sim_on ? "ENTER: relay thật bật/tắt" : "UP/DOWN: trang · EXIT: về";
}

const ui_screen_t scr_outputs = {
    .title = "TRẠNG THÁI ĐẦU RA", .page = 5, .refresh_ms = 200,
    .draw_static = draw_static, .draw_values = draw_values, .key = key, .hint = hint,
};
