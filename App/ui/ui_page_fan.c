/**
 * @file    ui_page_fan.c
 * @brief   Trang 3 – chu trình sấy 5 giai đoạn + cấp quạt dàn nóng từng giai đoạn.
 *          ENTER → danh sách cài đặt (cấp quạt từng giai đoạn, độ ẩm + thời gian giữ ẩm GĐ3/GĐ4,
 *          nhiệt độ dừng GĐ5, máy nén chờ bật lại, nhiệt độ bảo vệ).
 *
 *   Chu trình 5 giai đoạn             cấp quạt
 *   GĐ1  Gia nhiệt tới 55°C           cấp 4
 *   GĐ2  Hút ẩm tới 15%               cấp 3
 *   GĐ3  Ẩm ≤12% giữ 60 phút          cấp 2
 *   GĐ4  Ẩm ≤10% giữ 30 phút          cấp 1
 *   GĐ5  Làm mát tới 40°C             cấp 5
 *   Bảo vệ 75°C · Máy nén chờ 60 s
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
    struct { float ts, hs, et, tm, h3, h4; uint16_t g3, g4, cr; uint8_t st, s, f[5]; } sig;
    memset(&sig, 0, sizeof(sig));
    sig.ts = v->temp_set; sig.hs = v->hum_set; sig.et = v->end_temp; sig.tm = v->temp_max;
    sig.h3 = v->hum_gd3;  sig.h4 = v->hum_gd4;
    sig.g3 = v->gd3_min;  sig.g4 = v->gd4_min; sig.cr = v->comp_restart_s;
    sig.st = (uint8_t)v->stage; sig.s = (uint8_t)v->state;
    memcpy(sig.f, v->stage_fan, sizeof(sig.f));
    static uint8_t last[sizeof(sig)];
    if (!full && memcmp(last, &sig, sizeof(sig)) == 0) return;
    memcpy(last, &sig, sizeof(sig));

    w_label(8, Y_MODE, "Chu trình 5 giai đoạn");
    w_label(X_LVL - 30, Y_MODE, "cấp quạt");

    Fmt_Float(t, sizeof(t), v->temp_set, 0);
    Fmt_Float(h, sizeof(h), v->hum_set, 0);
    snprintf(a, sizeof(a), "GĐ1  Gia nhiệt tới %s°C", t);          row(0, a, v->stage_fan[0], v->stage == 0);
    snprintf(a, sizeof(a), "GĐ2  Hút ẩm tới %s%%", h);             row(1, a, v->stage_fan[1], v->stage == 1);
    Fmt_Float(h, sizeof(h), v->hum_gd3, 0);
    snprintf(a, sizeof(a), "GĐ3  Ẩm %s%% giữ %u phút", h, v->gd3_min); row(2, a, v->stage_fan[2], v->stage == 2);
    Fmt_Float(h, sizeof(h), v->hum_gd4, 0);
    snprintf(a, sizeof(a), "GĐ4  Ẩm %s%% giữ %u phút", h, v->gd4_min); row(3, a, v->stage_fan[3], v->stage == 3);
    Fmt_Float(t, sizeof(t), v->end_temp, 0);
    snprintf(a, sizeof(a), "GĐ5  Làm mát tới %s°C", t);           row(4, a, v->stage_fan[4], v->stage == 4);

    Fmt_Float(t, sizeof(t), v->temp_max, 0);
    snprintf(a, sizeof(a), "Bảo vệ %s°C · Máy nén chờ %u s", t, v->comp_restart_s);
    w_text(8, Y_PROT, TFT_WIDTH - 16, a, UC_LABEL, UC_BG, TEXT_LEFT);
}

static void key(ui_key_t k, ui_press_t p)
{
    if (k == UI_KEY_ENTER && p == UI_PRESS_SHORT) {
        ui_list_open("CÀI ĐẶT CHU TRÌNH", g_ui.cfg->process, SCR_FAN, true);
        return;
    }
    ui_page_nav(k, p);
}

static const char *hint(void)
{
    return "ENTER: cài đặt · EXIT: về chính";
}

const ui_screen_t scr_fan = {
    .title = "CHU TRÌNH SẤY", .page = 2, .refresh_ms = 500,
    .draw_values = draw_values, .key = key, .hint = hint,
};
