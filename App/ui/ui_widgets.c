/**
 * @file    ui_widgets.c
 * @brief   Hàm vẽ và định dạng dùng chung cho các màn hình.
 */
#include "ui_internal.h"

void w_text(int16_t x, int16_t y, const char *s, uint8_t width, uint16_t fg, uint16_t bg, uint8_t scale)
{
    char buf[40];
    size_t n = strlen(s);
    if (width == 0) width = (uint8_t)n;
    if (width >= sizeof(buf)) width = sizeof(buf) - 1;
    if (n > width) n = width;
    memcpy(buf, s, n);
    memset(buf + n, ' ', width - n);          /* đệm trắng để xoá chữ cũ */
    buf[width] = '\0';
    ILI9341_DrawString(x, y, buf, fg, bg, scale);
}

void w_text_center(int16_t y, const char *s, uint8_t width, uint16_t fg, uint16_t bg, uint8_t scale)
{
    /* căn giữa trong ô width ký tự, ô đặt giữa màn hình */
    char buf[40];
    size_t n = strlen(s);
    if (width >= sizeof(buf)) width = sizeof(buf) - 1;
    if (n > width) n = width;
    size_t left = (width - n) / 2;
    memset(buf, ' ', width);
    memcpy(buf + left, s, n);
    buf[width] = '\0';
    int16_t x = (int16_t)((TFT_WIDTH - (int16_t)width * 6 * scale) / 2);
    ILI9341_DrawString(x, y, buf, fg, bg, scale);
}

void w_hline(int16_t y)
{
    ILI9341_DrawHLine(8, y, TFT_WIDTH - 16, UC_LINE);
}

void w_badge(int16_t x, int16_t y, int16_t w, const char *s, bool on)
{
    uint16_t bg = on ? UC_OK : UC_OFF;
    uint16_t fg = on ? C_BLACK : UC_LABEL;
    ILI9341_FillRect(x, y, w, 20, bg);
    int16_t tx = (int16_t)(x + (w - (int16_t)strlen(s) * 12) / 2);
    ILI9341_DrawString(tx, (int16_t)(y + 2), s, fg, bg, 2);
}

const char *w_state_name(ui_state_t s)
{
    switch (s) {
    case UI_ST_IDLE:     return "DANG DUNG";
    case UI_ST_STARTING: return "KHOI DONG";
    case UI_ST_RUNNING:  return "DANG SAY";
    case UI_ST_STOPPING: return "DANG TAT";
    case UI_ST_FAULT:    return "LOI";
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

uint32_t w_remaining_s(void)
{
    uint32_t total = (uint32_t)g_ui.v.dry_time_min * 60UL;
    if (total == 0) return 0;
    return (g_ui.v.run_s < total) ? total - g_ui.v.run_s : 0;
}
