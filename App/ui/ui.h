/**
 * @file    ui.h
 * @brief   Giao diện TFT 320x240 – độc lập với điều khiển / cảm biến / settings.
 *          - app điền dữ liệu hiển thị vào ui_view_t và gọi UI_Update() mỗi ~100 ms
 *          - app chuyển nút bấm thành UI_Key()
 *          - UI gửi yêu cầu người dùng về app qua callback on_cmd(ui_cmd_t)
 *
 *  Điều hướng
 *  ──────────
 *  Trang 1 CHÍNH ⇄ 2 CHẠY/DỪNG ⇄ 3 THỜI GIAN SẤY ⇄ 4 QUẠT DÀN NÓNG ⇄ 5 LỊCH SỬ LỖI
 *     UP / DOWN : chuyển trang (vòng tròn)      EXIT : về trang chính
 *     Giữ ENTER 3 s (ở bất kỳ trang nào) → MENU CHẾ ĐỘ SẤY
 *     Giữ EXIT 3 s ở trang chính → MENU KỸ THUẬT (thông số bảo vệ)
 *
 *  Trang 2: ENTER = chạy / dừng; giữ EXIT 3 s = GIẢ LẬP (mô hình / chỉnh tay, tua nhanh, tạo lỗi)
 *  Trang 3: ENTER = chỉnh thời gian sấy HH:MM (00:00 = không giới hạn; chỉ dùng cho chế độ Tự động)
 *  Trang 4: ENTER = danh sách cài đặt: Tự động/Thủ công, cấp quạt từng GĐ, thời gian GĐ3/GĐ4,
 *           nhiệt độ dừng GĐ5, máy nén chờ bật lại, nhiệt độ bảo vệ
 *  Trang 5: ENTER = xoá lỗi đang có; giữ EXIT 3 s = xoá lịch sử
 *
 *  Menu chế độ: 6 chế độ đặt sẵn + "Tự do" + "Chỉnh đồng hồ"
 *     UP / DOWN chọn · ENTER áp dụng chế độ · EXIT thoát
 *     Giữ ENTER 3 s trên chế độ đặt sẵn → sửa giá trị của chế độ đó
 *     ENTER trên "Tự do" → nhập nhiệt độ, độ ẩm
 *  Nhập số: từng chữ số từ trái sang phải; UP/DOWN đổi số, ENTER sang số kế,
 *     ENTER ở số cuối = lưu, EXIT = huỷ.
 */
#ifndef UI_H
#define UI_H

#include <stdbool.h>
#include <stdint.h>

#define UI_PRESET_MAX   8        /* tối đa số chế độ (kể cả "Tự do") */
#define UI_OUT_MAX      7        /* số đầu ra relay hiển thị */
#define UI_HIST_ROWS    6        /* số dòng lịch sử lỗi hiển thị */

/* ---------------- Phím ---------------- */
typedef enum { UI_KEY_UP = 0, UI_KEY_DOWN, UI_KEY_ENTER, UI_KEY_EXIT } ui_key_t;
typedef enum { UI_PRESS_SHORT = 0, UI_PRESS_LONG, UI_PRESS_REPEAT } ui_press_t;

/* ---------------- Lệnh UI → app ---------------- */
typedef enum {
    UI_CMD_START_STOP = 0,
    UI_CMD_RESET_FAULT,
    UI_CMD_CLEAR_HISTORY,
    UI_CMD_SELECT_PRESET,      /* u.preset.idx */
    UI_CMD_SET_PRESET,         /* u.preset.idx, temp, hum, select */
    UI_CMD_SET_DRY_TIME,       /* u.minutes */
    UI_CMD_SET_CLOCK,          /* u.clock */
    UI_CMD_SAVE_SETTINGS,      /* sau khi sửa danh sách thông số */
    UI_CMD_SIM_RELAY,          /* giả lập: đảo "Relay thật" bật/tắt */
} ui_cmd_type_t;

typedef struct {
    ui_cmd_type_t type;
    union {
        struct { uint8_t idx; float temp; float hum; bool select; } preset;
        uint16_t minutes;
        struct { uint16_t year; uint8_t mon, day, hour, min; } clock;
    } u;
} ui_cmd_t;

/* ---------------- Dữ liệu hiển thị ---------------- */
typedef enum { UI_ST_IDLE = 0, UI_ST_STARTING, UI_ST_RUNNING, UI_ST_STOPPING, UI_ST_FAULT } ui_state_t;

typedef struct {
    char        when[12];        /* "27/09 14:05" */
    const char *text;            /* "Áp suất cao" */
} ui_hist_row_t;

typedef struct {
    /* đo */
    bool  temp_ok;  float temp;
    bool  hum_ok;   float hum;
    bool  press_ok; float press;
    /* điểm đặt & chế độ */
    float   temp_set, hum_set;
    bool    hum_reached;                     /* đã đạt độ ẩm mục tiêu (tô xanh) */
    uint8_t preset;                          /* chế độ đang dùng */
    float   preset_temp[UI_PRESET_MAX];
    float   preset_hum[UI_PRESET_MAX];
    /* trạng thái máy */
    ui_state_t state;
    bool     comp, fan_evap;
    uint8_t  fan_level;                      /* cấp quạt dàn nóng đang chạy 0..5 */
    const char *phase_text;                  /* "GĐ2 – hút ẩm"… (NULL/"" khi không chạy) */
    int8_t   stage;                          /* giai đoạn thủ công đang chạy 0..4, -1 = không */
    uint32_t run_s;                          /* đã sấy (s) */
    uint32_t phase_left_s;                   /* còn lại của GĐ3/GĐ4 */
    uint16_t dry_time_min;                   /* thời gian sấy đặt, 0 = không giới hạn */
    uint32_t comp_wait_s;                    /* > 0: máy nén đang chờ */
    const char *fault_text;                  /* NULL = không lỗi */
    const char *warn_text;                   /* NULL = không cảnh báo */
    /* điều khiển quạt dàn nóng / chu trình */
    bool     manual;                         /* chế độ thủ công (đang chạy hoặc đã chọn) */
    uint8_t  auto_fan;
    uint8_t  stage_fan[5];
    uint16_t gd3_min, gd4_min;
    float    end_temp, temp_max;
    uint16_t comp_restart_s;
    /* đầu ra: bit i = đầu ra i (thứ tự như cfg->output_names) */
    uint8_t  out_cmd;                        /* bộ điều khiển yêu cầu BẬT */
    uint8_t  out_relay;                      /* relay thật đang đóng */
    /* giả lập */
    const char *sim_text;                    /* != NULL: đang chạy giả lập (header đổi màu) */
    /* đồng hồ */
    bool     clock_ok;
    uint16_t year; uint8_t mon, day, hour, min, sec;
    /* lịch sử lỗi (mới nhất trước) */
    uint8_t       hist_count;
    ui_hist_row_t hist[UI_HIST_ROWS];
} ui_view_t;

/* ---------------- Danh sách thông số (menu kỹ thuật, cài đặt chu trình) ---------------- */
typedef struct {
    const char *name, *unit;
    float min, max, step;
    uint8_t dec;
    const char *const *choices;   /* != NULL: hiện chữ choices[giá trị] thay cho số */
} ui_param_desc_t;

typedef struct {
    uint8_t (*count)(void);
    bool    (*desc)(uint8_t idx, ui_param_desc_t *out);
    float   (*get)(uint8_t idx);
    void    (*set)(uint8_t idx, float v);
} ui_param_if_t;

/* ---------------- Cấu hình khởi tạo ---------------- */
typedef struct {
    const char *const   *preset_names;       /* UTF-8, phần tử cuối = "Tự do" */
    uint8_t              preset_count;       /* ≤ UI_PRESET_MAX */
    const ui_param_if_t *tech;               /* menu kỹ thuật (ẩn) */
    const ui_param_if_t *process;            /* cài đặt quạt dàn nóng / chu trình */
    const ui_param_if_t *sim;                /* chạy giả lập (không lưu Flash) */
    bool (*on_cmd)(const ui_cmd_t *cmd);     /* true = thành công */
    const char *const   *output_names;       /* tên đầu ra (trang trạng thái đầu ra) */
    const char *const   *output_pins;        /* ghi chú chân, có thể NULL */
    uint8_t              output_count;       /* ≤ UI_OUT_MAX */
} ui_config_t;

void UI_Init(const ui_config_t *cfg);
void UI_Key(ui_key_t key, ui_press_t press);
void UI_Update(const ui_view_t *view, uint32_t now_ms);
void UI_Message(const char *msg);            /* thông báo 2 s ở dòng trạng thái */

#endif
