/**
 * @file    ui_page_fan.c
 * @brief   Trang 4 – điều khiển quạt dàn nóng / chu trình sấy (Tự động / Thủ công).
 *          ENTER → danh sách cài đặt (chế độ, cấp quạt từng giai đoạn, thời gian GĐ3/GĐ4,
 *          nhiệt độ dừng GĐ5, máy nén chờ bật lại, nhiệt độ bảo vệ).
 *
 *   Chế độ:  Thủ công                    │   Chế độ:  Tự động
 *   GĐ1  Gia nhiệt tới 55°C     cấp 4    │   Quạt dàn nóng           cấp 3
 *   GĐ2  Hút ẩm tới 15%         cấp 3    │   1. Máy nén chạy tới độ ẩm 15%
 *   GĐ3  Giữ nhiệt 120 phút     cấp 2    │   2. Giữ nhiệt 55°C tới hết giờ sấy
 *   GĐ4  Giữ nhiệt 60 phút      cấp 1    │   3. Làm mát tới 40°C rồi dừng
 *   GĐ5  Làm mát tới 40°C       cấp 5    │
 *   Bảo vệ 75°C · Máy nén chờ 60 s        │
 */
#include "ui_internal.h"

#define Y_MODE   30
#define Y_ROW0   56
#define X_LVL    250
#define Y_PROT   (UI_FOOT_Y - LINE_H - 2)

static void row(uint8_t i, const char *txt, uint8_t lvl, bool active)
{
    int16_t y = (int16_t)(Y_ROW0 + i * LINE_H);
    uint16_t bg = active ? UC_SEL : UC_BG;
    uint16_t fg = active ? C_WHITE : UC_VALUE;
    char l[12];
    ILI9341_FillRect(0, y, 8, LINE_H, bg);
    w_text(8, y, X_LVL - 8, txt, fg, bg, TEXT_LEFT);
    if (lvl) snprintf(l, sizeof(l), "cấp %u", (unsigned)lvl); else l[0] = '\0';
    w_text(X_LVL, y, TFT_WIDTH - 8 - X_LVL, l, active ? UC_ACCENT : UC_LABEL, bg, TEXT_RIGHT);
    ILI9341_FillRect(TFT_WIDTH - 8, y, 8, LINE_H, bg);
}

static void draw_values(bool full)
{
    const ui_view_t *v = &g_ui.v;
    char a[64], t[12], h[12];

    /* chỉ vẽ lại khi nội dung đổi (giảm tải SPI) */
    struct { float ts, hs, et, tm; uint16_t g3, g4, cr; uint8_t m, st, s, af, f[5]; } sig;
    memset(&sig, 0, sizeof(sig));
    sig.ts = v->temp_set; sig.hs = v->hum_set; sig.et = v->end_temp; sig.tm = v->temp_max;
    sig.g3 = v->gd3_min;  sig.g4 = v->gd4_min; sig.cr = v->comp_restart_s;
    sig.m = v->manual; sig.st = (uint8_t)v->stage; sig.s = (uint8_t)v->state; sig.af = v->auto_fan;
    memcpy(sig.f, v->stage_fan, sizeof(sig.f));
    static uint8_t last[sizeof(sig)];
    if (!full && memcmp(last, &sig, sizeof(sig)) == 0) return;
    memcpy(last, &sig, sizeof(sig));

    w_label(8, Y_MODE, "Chế độ:");
    w_text(80, Y_MODE, TFT_WIDTH - 88, v->manual ? "Thủ công (5 giai đoạn)" : "Tự động",
           UC_ACCENT, UC_BG, TEXT_LEFT);

    Fmt_Float(t, sizeof(t), v->temp_set, 0);
    Fmt_Float(h, sizeof(h), v->hum_set, 0);
    if (v->manual) {
        snprintf(a, sizeof(a), "GĐ1  Gia nhiệt tới %s°C", t);          row(0, a, v->stage_fan[0], v->stage == 0);
        snprintf(a, sizeof(a), "GĐ2  Hút ẩm tới %s%%", h);             row(1, a, v->stage_fan[1], v->stage == 1);
        snprintf(a, sizeof(a), "GĐ3  Giữ nhiệt %u phút", v->gd3_min);  row(2, a, v->stage_fan[2], v->stage == 2);
        snprintf(a, sizeof(a), "GĐ4  Giữ nhiệt %u phút", v->gd4_min);  row(3, a, v->stage_fan[3], v->stage == 3);
        Fmt_Float(t, sizeof(t), v->end_temp, 0);
        snprintf(a, sizeof(a), "GĐ5  Làm mát tới %s°C", t);           row(4, a, v->stage_fan[4], v->stage == 4);
    } else {
        bool run = (v->state == UI_ST_RUNNING);
        row(0, "Quạt dàn nóng", v->auto_fan, false);
        snprintf(a, sizeof(a), "1. Máy nén chạy tới độ ẩm %s%%", h);   row(1, a, 0, run && v->stage == 0);
        snprintf(a, sizeof(a), "2. Giữ %s°C tới hết giờ sấy", t);      row(2, a, 0, run && v->stage == 1);
        Fmt_Float(t, sizeof(t), v->end_temp, 0);
        snprintf(a, sizeof(a), "3. Làm mát tới %s°C rồi dừng", t);     row(3, a, 0, run && v->stage == 2);
        row(4, "", 0, false);
    }
    Fmt_Float(t, sizeof(t), v->temp_max, 0);
    snprintf(a, sizeof(a), "Bảo vệ %s°C · Máy nén chờ %u s", t, v->comp_restart_s);
    w_text(8, Y_PROT, TFT_WIDTH - 16, a, UC_LABEL, UC_BG, TEXT_LEFT);
}

static void key(ui_key_t k, ui_press_t p)
{
    if (k == UI_KEY_ENTER && p == UI_PRESS_SHORT) {
        ui_list_open("CÀI ĐẶT CHU TRÌNH", g_ui.cfg->process, SCR_FAN);
        return;
    }
    ui_page_nav(k, p);
}

static const char *hint(void)
{
    return "ENTER: cài đặt · EXIT: về chính";
}

const ui_screen_t scr_fan = {
    .title = "QUẠT DÀN NÓNG", .page = 3, .refresh_ms = 500,
    .draw_values = draw_values, .key = key, .hint = hint,
};
