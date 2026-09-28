/**
 * @file    ui_widgets.c
 * @brief   Hàm vẽ và định dạng dùng chung cho các màn hình.
 */
#include "ui_internal.h"

void w_text(int16_t x, int16_t y, int16_t w, const char *s, uint16_t fg, uint16_t bg, text_align_t a)
{
    Text_Box(x, y, w, s, F_TXT, fg, bg, a);
}

void w_num(int16_t x, int16_t y, int16_t w, const char *s, uint16_t fg, uint16_t bg, text_align_t a)
{
    Text_Box(x, y, w, s, F_NUM, fg, bg, a);
}

void w_label(int16_t x, int16_t y, const char *s)
{
    Text_Draw(x, y, s, F_TXT, UC_LABEL, UC_BG);
}

void w_hline(int16_t y)
{
    ILI9341_DrawHLine(8, y, TFT_WIDTH - 16, UC_LINE);
}

void w_badge(int16_t x, int16_t y, int16_t w, const char *s, bool on)
{
    uint16_t bg = on ? UC_OK : UC_OFF;
    uint16_t fg = on ? C_BLACK : UC_LABEL;
    Text_Box(x, y, w, s, F_TXT, fg, bg, TEXT_CENTER);
}

const char *w_state_name(ui_state_t s)
{
    switch (s) {
    case UI_ST_IDLE:     return "Đang dừng";
    case UI_ST_STARTING: return "Khởi động";
    case UI_ST_RUNNING:  return "Đang sấy";
    case UI_ST_STOPPING: return "Đang tắt";
    case UI_ST_FAULT:    return "Lỗi";
    default:             return "?";
    }
}

uint16_t w_state_color(ui_state_t s)
{
    switch (s) {
    case UI_ST_RUNNING:  return UC_OK;
    case UI_ST_STARTING:
    case UI_ST_STOPPING: return UC_WARN;
    case UI_ST_FAULT:    return UC_ERR;
    default:             return UC_LABEL;
    }
}

char *w_fmt_value(char *buf, size_t n, bool ok, float v)
{
    if (!ok) { snprintf(buf, n, "--.-"); return buf; }
    return Fmt_Float(buf, n, v, (v >= 99.95f || v <= -9.95f) ? 0 : 1);
}

char *w_fmt_hhmm(char *buf, size_t n, uint32_t minutes)
{
    snprintf(buf, n, "%02lu:%02lu", (unsigned long)(minutes / 60), (unsigned long)(minutes % 60));
    return buf;
}

char *w_fan_badge(char *buf, size_t n, uint8_t level)
{
    if (level) snprintf(buf, n, "QN%u", (unsigned)level);
    else       snprintf(buf, n, "QN");
    return buf;
}

uint32_t w_remaining_s(void)
{
    if (g_ui.v.manual) return g_ui.v.phase_left_s;
    uint32_t total = (uint32_t)g_ui.v.dry_time_min * 60UL;
    if (total == 0) return 0;
    return (g_ui.v.run_s < total) ? total - g_ui.v.run_s : 0;
}
