/**
 * @file    ui_menu_preset.c
 * @brief   Menu chế độ sấy: 6 chế độ đặt sẵn + "Tu do" + "Chinh dong ho".
 *          ENTER trên chế độ đặt sẵn   → áp dụng ngay
 *          Giữ ENTER trên chế độ đặt sẵn → sửa nhiệt độ/độ ẩm của chế độ đó
 *          ENTER trên "Tu do"          → nhập nhiệt độ, độ ẩm rồi áp dụng
 *          ENTER trên "Chinh dong ho"  → nhập ngày giờ
 */
#include "ui_internal.h"

#define ROW_Y0   32
#define ROW_H    23

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
    char line[48], val[16];

    ILI9341_FillRect(0, y, TFT_WIDTH, ROW_H, bg);
    if (is_clock_row(r)) {
        if (v->clock_ok) snprintf(val, sizeof(val), "%02u:%02u", v->hour, v->min);
        else             snprintf(val, sizeof(val), "--:--");
        snprintf(line, sizeof(line), "   %-14s %8s", "Chinh dong ho", val);
        ILI9341_DrawString(4, (int16_t)(y + 4), line, UC_LABEL, bg, 2);
        return;
    }
    bool active = (r == v->preset);
    snprintf(val, sizeof(val), "%2dC %2d%%", (int)(v->preset_temp[r] + 0.5f), (int)(v->preset_hum[r] + 0.5f));
    snprintf(line, sizeof(line), "%c%u %-12s %8s", active ? '*' : ' ', (unsigned)(r + 1),
             g_ui.cfg->preset_names[r], val);
    ILI9341_DrawString(4, (int16_t)(y + 4), line, active ? UC_OK : UC_VALUE, bg, 2);
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
            UI_Message(ok ? "DA AP DUNG TU DO" : "DA GIOI HAN 30-75C");
            ui_goto(SCR_MAIN);
            return;
        }
        UI_Message(ok ? "DA LUU CHE DO" : "DA GIOI HAN 30-75C");
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
        UI_Message(ui_send(&c) ? "DA CHINH GIO" : "NGAY GIO SAI!");
    }
    ui_goto(SCR_PRESET);
}

static void begin_preset_edit(uint8_t idx)
{
    char t[16];
    s_edit_idx = idx;
    snprintf(t, sizeof(t), "%02dC %02d%%", (int)(g_ui.v.preset_temp[idx] + 0.5f) % 100,
             (int)(g_ui.v.preset_hum[idx] + 0.5f) % 100);
    ui_edit_begin(g_ui.cfg->preset_names[idx], "Nhiet do   Do am", t, preset_done, SCR_PRESET);
}

static void begin_clock_edit(void)
{
    const ui_view_t *v = &g_ui.v;
    char t[20];
    if (v->clock_ok) snprintf(t, sizeof(t), "%02u/%02u/%02u %02u:%02u", v->day, v->mon, v->year % 100, v->hour, v->min);
    else             snprintf(t, sizeof(t), "01/01/26 00:00");
    ui_edit_begin("CHINH DONG HO", "Ngay/Thang/Nam Gio:Phut", t, clock_done, SCR_PRESET);
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
            char m[24];
            snprintf(m, sizeof(m), "CHON: %s", g_ui.cfg->preset_names[s_sel]);
            UI_Message(m);
            ui_goto(SCR_MAIN);
        }
        break;
    }
}

static const char *hint(void)
{
    if (is_clock_row(s_sel)) return "ENTER: chinh gio   EXIT: thoat";
    if (is_custom(s_sel))    return "ENTER: nhap gia tri   EXIT: thoat";
    return "ENTER: chon  Giu ENTER: sua  EXIT: thoat";
}

const ui_screen_t scr_preset = {
    .title = "CHE DO SAY", .page = -1, .refresh_ms = 1000,
    .enter = enter, .draw_values = draw_values, .key = key, .hint = hint,
};
