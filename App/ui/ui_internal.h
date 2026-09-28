/**
 * @file    ui_internal.h
 * @brief   Dùng chung NỘI BỘ giữa các file trong thư mục ui/ (không include từ ngoài).
 *
 *  Mỗi màn hình là 1 ui_screen_t trong 1 file riêng:
 *    ui_page_main.c   trang 1  – nhiệt độ, độ ẩm, chế độ, trạng thái
 *    ui_page_run.c    trang 2  – chạy / dừng
 *    ui_page_timer.c  trang 3  – thời gian sấy
 *    ui_page_faults.c trang 4  – lịch sử lỗi
 *    ui_menu_preset.c menu chế độ sấy + chỉnh đồng hồ
 *    ui_edit.c        nhập số từng chữ số
 *    ui_menu_tech.c   menu kỹ thuật
 *  ui_core.c điều phối vẽ header/footer và chuyển màn hình; ui_widgets.c là các hàm vẽ chung.
 */
#ifndef UI_INTERNAL_H
#define UI_INTERNAL_H

#include "ui.h"
#include "ili9341.h"
#include "ili9341_text.h"
#include "util_fmt.h"
#include <stdio.h>
#include <string.h>

/* ---------------- Bảng màu ---------------- */
#define UC_BG       C_BLACK
#define UC_HEAD     RGB565(18, 42, 84)
#define UC_FOOT     RGB565(26, 30, 36)
#define UC_LABEL    RGB565(140, 150, 162)
#define UC_VALUE    C_WHITE
#define UC_TEMP     RGB565(255, 150, 50)
#define UC_HUM      RGB565(90, 180, 255)
#define UC_ACCENT   RGB565(0, 210, 230)
#define UC_OK       RGB565(60, 210, 90)
#define UC_WARN     RGB565(250, 200, 40)
#define UC_ERR      RGB565(235, 60, 50)
#define UC_SEL      RGB565(40, 72, 128)
#define UC_CURSOR   RGB565(255, 150, 50)
#define UC_LINE     RGB565(60, 66, 76)
#define UC_OFF      RGB565(55, 60, 68)

/* ---------------- Bố cục ---------------- */
#define UI_HEAD_H   27
#define UI_FOOT_Y   217              /* footer cao 23 px = 1 dòng font_vn16 */
#define F_TXT       (&font_vn16)     /* chữ tiếng Việt */
#define F_NUM       (&font_num)      /* số lớn */
#define LINE_H      23               /* chiều cao dòng F_TXT */
#define UI_BODY_Y   UI_HEAD_H
#define UI_PAGE_COUNT 5
#define UI_MSG_MS   2000

typedef enum {
    SCR_MAIN = 0, SCR_RUN, SCR_TIMER, SCR_FAN, SCR_FAULTS,   /* 5 trang, chuyển bằng UP/DOWN */
    SCR_PRESET, SCR_EDIT, SCR_LIST,
    SCR_COUNT
} ui_scr_t;

typedef struct {
    const char *title;                          /* tiêu đề header */
    int8_t      page;                           /* 0..3 = số trang, -1 = không phải trang */
    uint16_t    refresh_ms;                     /* chu kỳ vẽ lại giá trị */
    void (*enter)(void);                        /* gọi khi chuyển tới (có thể NULL) */
    void (*draw_static)(void);                  /* phần cố định của thân */
    void (*draw_values)(bool full);             /* phần thay đổi (đọc g_ui.v) */
    void (*key)(ui_key_t k, ui_press_t p);
    const char *(*hint)(void);                  /* gợi ý phím ở footer */
} ui_screen_t;

typedef struct {
    const ui_config_t *cfg;
    ui_view_t  v;                               /* bản sao view mới nhất */
    ui_scr_t   scr;
    bool       redraw;                          /* vẽ lại toàn màn hình */
    bool       dirty;                           /* vẽ lại giá trị ngay */
    uint32_t   now, last_draw;
    char       msg[48];
    uint32_t   msg_tick;
    char       clock_drawn[16];
    char       foot_drawn[80];
} ui_ctx_t;

extern ui_ctx_t g_ui;

extern const ui_screen_t scr_main, scr_run, scr_timer, scr_fan, scr_faults;
extern const ui_screen_t scr_preset, scr_edit, scr_list;

/* ---------------- Core ---------------- */
void ui_goto(ui_scr_t s);
bool ui_send(const ui_cmd_t *cmd);
bool ui_page_nav(ui_key_t k, ui_press_t p);      /* phím chung cho 4 trang */

/* ---------------- Nhập số từng chữ số ---------------- */
/* text: chuỗi ban đầu, mọi ký tự '0'..'9' là ô nhập, ký tự khác giữ nguyên (vd "27/09/26 14:05") */
typedef void (*ui_edit_done_t)(const uint8_t *digits, uint8_t count);
void ui_edit_begin(const char *title, const char *label, const char *text,
                   ui_edit_done_t done, ui_scr_t back);

/* ---------------- Danh sách thông số dùng chung (ui_list.c) ---------------- */
void ui_list_open(const char *title, const ui_param_if_t *list, ui_scr_t back);

/* ---------------- Widgets (ui_widgets.c) ---------------- */
/* Ô chữ rộng w px: căn lề + tô nền phần thừa (xoá chữ cũ) */
void w_text(int16_t x, int16_t y, int16_t w, const char *s, uint16_t fg, uint16_t bg, text_align_t a);
void w_num(int16_t x, int16_t y, int16_t w, const char *s, uint16_t fg, uint16_t bg, text_align_t a);
void w_label(int16_t x, int16_t y, const char *s);                 /* nhãn tĩnh màu xám */
void w_hline(int16_t y);
void w_badge(int16_t x, int16_t y, int16_t w, const char *s, bool on);
const char *w_state_name(ui_state_t s);
uint16_t    w_state_color(ui_state_t s);
char *w_fmt_value(char *buf, size_t n, bool ok, float v);        /* "55.3" / "--.-" */
char *w_fmt_hhmm(char *buf, size_t n, uint32_t minutes);         /* "12:30" */
uint32_t w_remaining_s(void);        /* còn lại: tự động = thời gian sấy, thủ công = GĐ3/GĐ4; 0 nếu không */
char *w_fan_badge(char *buf, size_t n, uint8_t level);           /* "Q3" / "Q–" */

#endif
