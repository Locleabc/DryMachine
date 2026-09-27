/**
 * @file    ui_page_run.c
 * @brief   Trang 2 – chạy / dừng máy sấy.
 */
#include "ui_internal.h"

#define Y_STATE   42
#define Y_ELAPSED 94
#define Y_REMAIN  118
#define Y_COMP    142
#define Y_BTN     170

static void draw_static(void)
{
    ILI9341_DrawString(8, Y_ELAPSED, "Da say:",  UC_LABEL, UC_BG, 2);
    ILI9341_DrawString(8, Y_REMAIN,  "Con lai:", UC_LABEL, UC_BG, 2);
    ILI9341_DrawString(8, Y_COMP,    "May nen:", UC_LABEL, UC_BG, 2);
}

static void draw_values(bool full)
{
    const ui_view_t *v = &g_ui.v;
    char a[24], b[12];

    w_text_center(Y_STATE, w_state_name(v->state), 12, w_state_color(v->state), UC_BG, 3);

    bool running = (v->state == UI_ST_RUNNING || v->state == UI_ST_STARTING);
    w_text(116, Y_ELAPSED, running ? Fmt_Time(b, sizeof(b), v->run_s) : "--:--:--", 12, UC_VALUE, UC_BG, 2);

    if (v->dry_time_min == 0)  snprintf(a, sizeof(a), "khong gioi han");
    else if (running)          Fmt_Time(a, sizeof(a), w_remaining_s());
    else                       snprintf(a, sizeof(a), "%s (dat)", w_fmt_hhmm(b, sizeof(b), v->dry_time_min));
    w_text(116, Y_REMAIN, a, 16, UC_VALUE, UC_BG, 2);

    if (v->comp)                 snprintf(a, sizeof(a), "CHAY");
    else if (running && v->comp_wait_s) snprintf(a, sizeof(a), "cho %lus", (unsigned long)v->comp_wait_s);
    else                         snprintf(a, sizeof(a), "TAT");
    w_text(116, Y_COMP, a, 16, v->comp ? UC_OK : UC_VALUE, UC_BG, 2);

    /* nút hành động – chỉ vẽ lại khi trạng thái đổi */
    static ui_state_t last = (ui_state_t)0xFF;
    if (!full && last == v->state) return;
    last = v->state;
    const char *txt; uint16_t bg, fg = C_BLACK;
    switch (v->state) {
    case UI_ST_IDLE:     txt = "ENTER: BAT DAU SAY"; bg = UC_OK;   break;
    case UI_ST_STARTING:
    case UI_ST_RUNNING:  txt = "ENTER: DUNG SAY";    bg = UC_ERR;  fg = C_WHITE; break;
    case UI_ST_STOPPING: txt = "DANG TAT QUAT...";   bg = UC_OFF;  fg = UC_LABEL; break;
    default:             txt = "LOI - XEM TRANG 4";  bg = UC_OFF;  fg = UC_ERR;  break;
    }
    ILI9341_FillRect(8, Y_BTN, TFT_WIDTH - 16, 40, bg);
    w_text_center(Y_BTN + 12, txt, 20, fg, bg, 2);
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
    return "ENTER: chay/dung   EXIT: ve chinh";
}

const ui_screen_t scr_run = {
    .title = "CHAY / DUNG", .page = 1, .refresh_ms = 500,
    .draw_static = draw_static, .draw_values = draw_values, .key = key, .hint = hint,
};
