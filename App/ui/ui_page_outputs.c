/**
 * @file    ui_page_outputs.c
 * @brief   Trang 6 – trạng thái các đầu ra relay (ON / OFF) + test bật/tắt tay.
 *
 *  Tên + chân lấy từ cfg->output_names / output_pins (app.c khai báo, UI không biết phần cứng).
 *  Mỗi đầu ra có 2 bit trong view:
 *    out_cmd   – đang được yêu cầu BẬT (bộ điều khiển hoặc test)
 *    out_relay – relay thật đang đóng
 *  Yêu cầu BẬT nhưng relay chưa đóng (giả lập Relay thật = Tắt / đang nghỉ đổi cấp quạt) → ô vàng "ON*".
 *
 *  Dòng dưới cùng: nhiệt độ · độ ẩm · áp suất hiện tại.
 *
 *  Test đầu ra (chỉ khi máy không chạy – app kiểm tra):
 *    ENTER        vào test
 *    UP / DOWN    chọn đầu ra
 *    ENTER        bật / tắt đầu ra đang chọn
 *    EXIT         thoát test, tắt hết
 */
#include "ui_internal.h"

#define Y0       29
#define X_NAME   8
#define X_PIN    160
#define X_PILL   248
#define W_PILL   64
#define Y_NOTE   (Y0 + UI_OUT_MAX * LINE_H + 2)

static bool    s_test;      /* UI đang ở chế độ test */
static uint8_t s_cur;       /* dòng đang chọn */

static uint8_t out_count(void)
{
    uint8_t n = g_ui.cfg->output_count;
    return (n > UI_OUT_MAX) ? UI_OUT_MAX : n;
}

static bool send_test(uint8_t op, uint8_t idx)
{
    ui_cmd_t c = { .type = UI_CMD_OUT_TEST };
    c.u.test.op = op;
    c.u.test.idx = idx;
    return ui_send(&c);
}

static void enter(void)
{
    s_test = false;
    s_cur = 0;
}

static void draw_row_label(uint8_t i, bool sel)
{
    const ui_config_t *c = g_ui.cfg;
    int16_t y = (int16_t)(Y0 + i * LINE_H);
    uint16_t bg = sel ? UC_SEL : UC_BG;
    w_text(0, y, X_NAME, "", UC_VALUE, bg, TEXT_LEFT);
    w_text(X_NAME, y, X_PIN - X_NAME, c->output_names[i], UC_VALUE, bg, TEXT_LEFT);
    w_text(X_PIN, y, X_PILL - X_PIN - 4, c->output_pins ? c->output_pins[i] : "", UC_LABEL, bg, TEXT_LEFT);
}

static void draw_values(bool full)
{
    const ui_view_t *v = &g_ui.v;
    static uint8_t last_cmd = 0xFF, last_rly = 0xFF, last_cur = 0xFF;
    static bool last_test;
    static char last_meas[3][12];
    char meas[3][12], n[8];

    if (s_test && !v->out_test) s_test = false;          /* app đã thoát test (hết giờ / máy chạy) */

    /* dòng số đo */
    w_fmt_value(n, sizeof(n), v->temp_ok, v->temp);   snprintf(meas[0], sizeof(meas[0]), "%s°C", n);
    w_fmt_value(n, sizeof(n), v->hum_ok, v->hum);     snprintf(meas[1], sizeof(meas[1]), "%s%%", n);
    w_fmt_value(n, sizeof(n), v->press_ok, v->press); snprintf(meas[2], sizeof(meas[2]), "%s bar", n);
    bool meas_chg = full || memcmp(meas, last_meas, sizeof(meas)) != 0;
    if (meas_chg) {
        memcpy(last_meas, meas, sizeof(meas));
        w_text(X_NAME,        Y_NOTE, 96,  meas[0], UC_TEMP,  UC_BG, TEXT_LEFT);
        w_text(X_NAME + 96,   Y_NOTE, 96,  meas[1], UC_HUM,   UC_BG, TEXT_LEFT);
        w_text(X_NAME + 192,  Y_NOTE, TFT_WIDTH - 16 - 192, meas[2], UC_VALUE, UC_BG, TEXT_LEFT);
    }

    bool labels = full || last_test != s_test || last_cur != s_cur;
    if (!labels && last_cmd == v->out_cmd && last_rly == v->out_relay) return;
    last_cmd = v->out_cmd;
    last_rly = v->out_relay;

    if (labels) {
        for (uint8_t i = 0; i < out_count(); i++) draw_row_label(i, s_test && i == s_cur);
        last_test = s_test;
        last_cur = s_cur;
    }

    for (uint8_t i = 0; i < out_count(); i++) {
        bool cmd = (v->out_cmd >> i) & 1u;
        bool rly = (v->out_relay >> i) & 1u;
        int16_t y = (int16_t)(Y0 + i * LINE_H);
        if (cmd && !rly) {                                  /* yêu cầu bật nhưng relay chưa/không đóng */
            Text_Box(X_PILL, y, W_PILL, "ON*", F_TXT, C_BLACK, UC_WARN, TEXT_CENTER);
        } else {
            w_badge(X_PILL, y, W_PILL, rly ? "ON" : "OFF", rly);
        }
    }
}

static void key(ui_key_t k, ui_press_t p)
{
    if (!s_test) {
        if (k == UI_KEY_ENTER && p == UI_PRESS_SHORT) {
            if (send_test(UI_TEST_BEGIN, 0)) { s_test = true; s_cur = 0; }
            return;
        }
        ui_page_nav(k, p);
        return;
    }

    /* đang test: khoá chuyển trang, giữ ENTER không mở menu chế độ */
    uint8_t n = out_count();
    switch (k) {
    case UI_KEY_UP:
        if (p != UI_PRESS_LONG && n) s_cur = (uint8_t)((s_cur + n - 1) % n);
        break;
    case UI_KEY_DOWN:
        if (p != UI_PRESS_LONG && n) s_cur = (uint8_t)((s_cur + 1) % n);
        break;
    case UI_KEY_ENTER:
        if (p == UI_PRESS_SHORT) send_test(UI_TEST_TOGGLE, s_cur);
        break;
    case UI_KEY_EXIT:
        if (p == UI_PRESS_SHORT) {
            send_test(UI_TEST_END, 0);
            s_test = false;
            UI_Message("Thoát test – đã tắt hết");
        }
        break;
    }
}

static const char *hint(void)
{
    return s_test ? "TEST · ENTER bật/tắt · EXIT thoát" : "ENTER: test đầu ra · EXIT: về";
}

const ui_screen_t scr_outputs = {
    .title = "TRẠNG THÁI ĐẦU RA", .page = 5, .refresh_ms = 200,
    .enter = enter, .draw_values = draw_values, .key = key, .hint = hint,
};
