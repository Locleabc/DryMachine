/**
 * @file    ui_page_faults.c
 * @brief   Trang 4 – lỗi hiện tại + lịch sử lỗi (mới nhất ở trên).
 */
#include "ui_internal.h"

#define Y_NOW    34
#define Y_LIST   70
#define ROW_H    24

static void draw_values(bool full)
{
    const ui_view_t *v = &g_ui.v;
    char a[32];
    static const char *last_fault = (const char *)1;
    static uint8_t last_count = 0xFF;
    static char last_top[12];
    /* chỉ vẽ lại khi lỗi hiện tại hoặc lịch sử thay đổi */
    if (!full && last_fault == v->fault_text && last_count == v->hist_count &&
        strcmp(last_top, v->hist_count ? v->hist[0].when : "") == 0) return;
    last_fault = v->fault_text;
    last_count = v->hist_count;
    snprintf(last_top, sizeof(last_top), "%s", v->hist_count ? v->hist[0].when : "");

    if (v->fault_text) {
        snprintf(a, sizeof(a), "DANG LOI: %s", v->fault_text);
        ILI9341_FillRect(0, Y_NOW - 2, TFT_WIDTH, 26, UC_ERR);
        w_text(8, Y_NOW + 3, a, 25, C_WHITE, UC_ERR, 2);
    } else {
        ILI9341_FillRect(0, Y_NOW - 2, TFT_WIDTH, 26, UC_BG);
        w_text(8, Y_NOW + 3, "Khong co loi", 25, UC_OK, UC_BG, 2);
    }

    for (uint8_t i = 0; i < UI_HIST_ROWS; i++) {
        int16_t y = (int16_t)(Y_LIST + i * ROW_H);
        if (i < v->hist_count && i < UI_HIST_ROWS) {
            w_text(8,   y, v->hist[i].when, 11, UC_LABEL, UC_BG, 2);
            w_text(146, y, v->hist[i].text ? v->hist[i].text : "?", 14, UC_VALUE, UC_BG, 2);
        } else if (i == 0) {
            w_text(8, y, "Chua co lich su", 25, UC_LABEL, UC_BG, 2);
        } else {
            w_text(8, y, "", 25, UC_BG, UC_BG, 2);
        }
    }
}

static void key(ui_key_t k, ui_press_t p)
{
    if (k == UI_KEY_ENTER && p == UI_PRESS_SHORT) {
        if (g_ui.v.fault_text) {
            ui_cmd_t c = { .type = UI_CMD_RESET_FAULT };
            ui_send(&c);
        }
        return;
    }
    if (k == UI_KEY_EXIT && p == UI_PRESS_LONG) {
        ui_cmd_t c = { .type = UI_CMD_CLEAR_HISTORY };
        UI_Message(ui_send(&c) ? "DA XOA LICH SU" : "LOI XOA!");
        return;
    }
    ui_page_nav(k, p);
}

static const char *hint(void)
{
    return g_ui.v.fault_text ? "ENTER: xoa loi  Giu EXIT: xoa LS"
                             : "Giu EXIT 3s: xoa lich su";
}

const ui_screen_t scr_faults = {
    .title = "LICH SU LOI", .page = 3, .refresh_ms = 500,
    .draw_values = draw_values, .key = key, .hint = hint,
};
