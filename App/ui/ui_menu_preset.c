/**
 * @file    ui_menu_preset.c
 * @brief   Menu chế độ sấy: 6 chế độ đặt sẵn + "Tự do" + "Chỉnh đồng hồ".
 *          ENTER trên chế độ đặt sẵn   → áp dụng ngay
 *          Giữ ENTER trên chế độ đặt sẵn → sửa nhiệt độ/độ ẩm của chế độ đó
 *          ENTER trên "Tự do"          → nhập nhiệt độ, độ ẩm rồi áp dụng
 *          ENTER trên "Chỉnh đồng hồ"  → nhập ngày giờ
 */
#include "ui_internal.h"

#define ROW_Y0   30
#define ROW_H    LINE_H
#define X_NAME   8
#define X_VAL    196
#define W_VAL    (TFT_WIDTH - 8 - X_VAL)

static uint8_t s_sel;          /* dòng đang chọn */
static uint8_t s_edit_idx;     /* chế độ đang sửa */

static uint8_t row_count(void)  { return (uint8_t)(g_ui.cfg->preset_count + 1); }   /* + dòng đồng hồ */
static bool    is_clock_row(uint8_t r) { return r == g_ui.cfg->preset_count; }
static bool    is_custom(uint8_t r)    { return r + 1 == g_ui.cfg->preset_count; }

static void draw_row(uint8_t r)
{
    const ui_view_t *v = &g_ui.v;
    int16_t y = (int16_t)(ROW_Y0 + r * ROW_H);
    bool sel = (r == s_sel);
    uint16_t bg = sel ? UC_SEL : UC_BG;
    char line[48], val[24];

    ILI9341_FillRect(0, y, X_NAME, ROW_H, bg);
    if (is_clock_row(r)) {
        if (v->clock_ok) snprintf(val, sizeof(val), "%02u:%02u", v->hour, v->min);
        else             snprintf(val, sizeof(val), "--:--");
        w_text(X_NAME, y, X_VAL - X_NAME, "   Chỉnh đồng hồ", UC_LABEL, bg, TEXT_LEFT);
        w_text(X_VAL, y, W_VAL, val, UC_LABEL, bg, TEXT_RIGHT);
    } else {
        bool active = (r == v->preset);
        uint16_t fg = active ? UC_OK : UC_VALUE;
        snprintf(line, sizeof(line), "%s%u  %s", active ? "•" : "  ", (unsigned)(r + 1), g_ui.cfg->preset_names[r]);
        snprintf(val, sizeof(val), "%d°C  %d%%", (int)(v->preset_temp[r] + 0.5f), (int)(v->preset_hum[r] + 0.5f));
        w_text(X_NAME, y, X_VAL - X_NAME, line, fg, bg, TEXT_LEFT);
        w_text(X_VAL, y, W_VAL, val, fg, bg, TEXT_RIGHT);
    }
    ILI9341_FillRect(TFT_WIDTH - 8, y, 8, ROW_H, bg);
}

static void enter(void)
{
    s_sel = (g_ui.v.preset < g_ui.cfg->preset_count) ? g_ui.v.preset : 0;
}

static void draw_values(bool full)
{
    static uint8_t last_sel = 0xFF, last_min = 0xFF;
    if (full || last_sel != s_sel) {              /* chỉ vẽ lại danh sách khi đổi dòng */
        last_sel = s_sel;
        for (uint8_t r = 0; r < row_count(); r++) draw_row(r);
    } else if (last_min != g_ui.v.min) {
        draw_row(g_ui.cfg->preset_count);         /* dòng đồng hồ */
    }
    last_min = g_ui.v.min;
}

/* ---- callback nhập số ---- */
static void preset_done(const uint8_t *d, uint8_t n)
{
    if (n >= 4) {
        ui_cmd_t c = { .type = UI_CMD_SET_PRESET };
        c.u.preset.idx    = s_edit_idx;
        c.u.preset.temp   = (float)(d[0] * 10 + d[1]);
        c.u.preset.hum    = (float)(d[2] * 10 + d[3]);
        c.u.preset.select = is_custom(s_edit_idx);
        bool ok = ui_send(&c);
        if (c.u.preset.select) {
            UI_Message(ok ? "Đã áp dụng chế độ Tự do" : "Đã giới hạn 30–75°C, 5–80%");
            ui_goto(SCR_MAIN);
            return;
        }
        UI_Message(ok ? "Đã lưu chế độ" : "Đã giới hạn 30–75°C, 5–80%");
    }
    ui_goto(SCR_PRESET);
}

static void clock_done(const uint8_t *d, uint8_t n)
{
    if (n >= 10) {
        ui_cmd_t c = { .type = UI_CMD_SET_CLOCK };
        c.u.clock.day  = (uint8_t)(d[0] * 10 + d[1]);
        c.u.clock.mon  = (uint8_t)(d[2] * 10 + d[3]);
        c.u.clock.year = (uint16_t)(2000 + d[4] * 10 + d[5]);
        c.u.clock.hour = (uint8_t)(d[6] * 10 + d[7]);
        c.u.clock.min  = (uint8_t)(d[8] * 10 + d[9]);
        UI_Message(ui_send(&c) ? "Đã chỉnh đồng hồ" : "Ngày giờ không hợp lệ!");
    }
    ui_goto(SCR_PRESET);
}

static void begin_preset_edit(uint8_t idx)
{
    char t[24];
    s_edit_idx = idx;
    snprintf(t, sizeof(t), "%02d°C %02d%%", (int)(g_ui.v.preset_temp[idx] + 0.5f) % 100,
             (int)(g_ui.v.preset_hum[idx] + 0.5f) % 100);
    ui_edit_begin(g_ui.cfg->preset_names[idx], "Nhiệt độ        Độ ẩm", t, preset_done, SCR_PRESET);
}

static void begin_clock_edit(void)
{
    const ui_view_t *v = &g_ui.v;
    char t[24];
    /* 2 dòng: ngày-tháng-năm / giờ:phút */
    if (v->clock_ok) snprintf(t, sizeof(t), "%02u-%02u-%02u\n%02u:%02u", v->day % 100u, v->mon % 100u,
                              v->year % 100u, v->hour % 100u, v->min % 100u);
    else             snprintf(t, sizeof(t), "01-01-26\n00:00");
    ui_edit_begin("CHỈNH ĐỒNG HỒ", "Ngày - Tháng - Năm  /  Giờ : Phút", t, clock_done, SCR_PRESET);
}

static void key(ui_key_t k, ui_press_t p)
{
    uint8_t n = row_count();
    switch (k) {
    case UI_KEY_UP:   if (p != UI_PRESS_LONG) s_sel = (uint8_t)((s_sel + n - 1) % n); break;
    case UI_KEY_DOWN: if (p != UI_PRESS_LONG) s_sel = (uint8_t)((s_sel + 1) % n);     break;
    case UI_KEY_EXIT: if (p == UI_PRESS_SHORT) ui_goto(SCR_MAIN);                      break;
    case UI_KEY_ENTER:
        if (is_clock_row(s_sel)) {
            if (p == UI_PRESS_SHORT) begin_clock_edit();
        } else if (is_custom(s_sel) || p == UI_PRESS_LONG) {
            begin_preset_edit(s_sel);                   /* "Tu do" hoặc giữ ENTER = sửa */
        } else if (p == UI_PRESS_SHORT) {
            ui_cmd_t c = { .type = UI_CMD_SELECT_PRESET };
            c.u.preset.idx = s_sel;
            ui_send(&c);
            char m[48];
            snprintf(m, sizeof(m), "Đã chọn: %s", g_ui.cfg->preset_names[s_sel]);
            UI_Message(m);
            ui_goto(SCR_MAIN);
        }
        break;
    }
}

static const char *hint(void)
{
    if (is_clock_row(s_sel)) return "ENTER: chỉnh giờ · EXIT: thoát";
    if (is_custom(s_sel))    return "ENTER: nhập giá trị · EXIT: thoát";
    return "ENTER: chọn · Giữ ENTER: sửa";
}

const ui_screen_t scr_preset = {
    .title = "CHẾ ĐỘ SẤY", .page = -1, .refresh_ms = 1000,
    .enter = enter, .draw_values = draw_values, .key = key, .hint = hint,
};
