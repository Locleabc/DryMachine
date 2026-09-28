/**
 * @file    ui_page_faults.c
 * @brief   Trang 4 – lỗi hiện tại + lịch sử lỗi (mới nhất ở trên).
 */
#include "ui_internal.h"

#define Y_NOW    31
#define Y_LIST   62
#define ROW_H    LINE_H
#define X_TXT    116

static void draw_values(bool full)
{
    const ui_view_t *v = &g_ui.v;
    char a[48];
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
        snprintf(a, sizeof(a), "Đang lỗi: %s", v->fault_text);
        ILI9341_FillRect(0, Y_NOW - 2, TFT_WIDTH, LINE_H + 4, UC_ERR);
        w_text(8, Y_NOW, TFT_WIDTH - 16, a, C_WHITE, UC_ERR, TEXT_LEFT);
    } else {
        ILI9341_FillRect(0, Y_NOW - 2, TFT_WIDTH, LINE_H + 4, UC_BG);
        w_text(8, Y_NOW, TFT_WIDTH - 16, "Không có lỗi", UC_OK, UC_BG, TEXT_LEFT);
    }

    for (uint8_t i = 0; i < UI_HIST_ROWS; i++) {
        int16_t y = (int16_t)(Y_LIST + i * ROW_H);
        if (i < v->hist_count) {
            w_text(8,     y, X_TXT - 8, v->hist[i].when, UC_LABEL, UC_BG, TEXT_LEFT);
            w_text(X_TXT, y, TFT_WIDTH - 8 - X_TXT, v->hist[i].text ? v->hist[i].text : "?", UC_VALUE, UC_BG, TEXT_LEFT);
        } else {
            w_text(8, y, TFT_WIDTH - 16, (i == 0) ? "Chưa có lịch sử lỗi" : "", UC_LABEL, UC_BG, TEXT_LEFT);
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
        UI_Message(ui_send(&c) ? "Đã xoá lịch sử" : "Lỗi xoá!");
        return;
    }
    ui_page_nav(k, p);
}

static const char *hint(void)
{
    return g_ui.v.fault_text ? "ENTER: xoá lỗi · Giữ EXIT: xoá LS"
                             : "Giữ EXIT 3s: xoá lịch sử";
}

const ui_screen_t scr_faults = {
    .title = "LỊCH SỬ LỖI", .page = 3, .refresh_ms = 500,
    .draw_values = draw_values, .key = key, .hint = hint,
};
