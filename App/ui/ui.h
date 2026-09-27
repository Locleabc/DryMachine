/**
 * @file    ui.h
 * @brief   Giao diện TFT. Độc lập với điều khiển/cảm biến/settings:
 *          - Nhận dữ liệu hiển thị qua ui_view_t (app điền)
 *          - Đọc/sửa thông số qua ui_param_if_t (app nối tới settings)
 *          - Gửi yêu cầu người dùng qua callback ui_cmd_cb_t (app xử lý)
 *
 *  Màn hình chính:  ENTER nhấn  → Menu cài đặt
 *                   ENTER giữ   → UI_CMD_START_STOP
 *                   EXIT  giữ   → UI_CMD_RESET_FAULT
 *  Menu:            UP/DOWN chọn, ENTER sửa, EXIT về (UI_CMD_SAVE_SETTINGS nếu có thay đổi)
 *  Sửa giá trị:     UP/DOWN tăng/giảm (giữ để lặp), ENTER xác nhận, EXIT huỷ
 */
#ifndef UI_H
#define UI_H

#include <stdbool.h>
#include <stdint.h>

typedef enum { UI_KEY_UP = 0, UI_KEY_DOWN, UI_KEY_ENTER, UI_KEY_EXIT } ui_key_t;
typedef enum { UI_PRESS_SHORT = 0, UI_PRESS_LONG, UI_PRESS_REPEAT } ui_press_t;

typedef enum {
    UI_CMD_START_STOP = 0,
    UI_CMD_RESET_FAULT,
    UI_CMD_SAVE_SETTINGS,
} ui_cmd_t;

typedef enum { UI_ALARM_NONE = 0, UI_ALARM_WARN, UI_ALARM_FAULT } ui_alarm_t;

/* Dữ liệu hiển thị ở màn hình chính */
typedef struct {
    bool  temp_ok;  float temp;  float temp_set;
    bool  hum_ok;   float hum;   float hum_set;  bool hum_reached;
    bool  press_ok; float press;
    bool  comp, fan_cond, fan_evap;
    const char *state_text;        /* "DANG SAY", "DUNG" … */
    ui_alarm_t  alarm;
    const char *alarm_text;        /* dùng khi alarm != NONE */
    bool     running;
    uint32_t run_s;
    uint32_t comp_wait_s;          /* > 0: đang chờ bật máy nén */
} ui_view_t;

/* Mô tả 1 thông số trong menu */
typedef struct {
    const char *name;
    const char *unit;
    float min, max, step;
    uint8_t dec;
} ui_param_desc_t;

/* Giao diện truy cập danh sách thông số */
typedef struct {
    uint8_t (*count)(void);
    bool    (*desc)(uint8_t idx, ui_param_desc_t *out);
    float   (*get)(uint8_t idx);
    void    (*set)(uint8_t idx, float v);
} ui_param_if_t;

/* Trả về true nếu lệnh thực hiện thành công (dùng để báo "DA LUU" / "LOI") */
typedef bool (*ui_cmd_cb_t)(ui_cmd_t cmd);

void UI_Init(const ui_param_if_t *params, ui_cmd_cb_t on_cmd);
void UI_Key(ui_key_t key, ui_press_t press);
void UI_Update(const ui_view_t *view, uint32_t now_ms);     /* gọi mỗi ~100 ms */
void UI_Message(const char *msg);                            /* thông báo 2 s ở dòng trạng thái */

#endif
