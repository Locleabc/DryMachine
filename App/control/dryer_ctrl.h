/**
 * @file    dryer_ctrl.h
 * @brief   Logic điều khiển máy sấy – THUẦN C: không HAL, không driver, không biến toàn cục.
 *          Đầu vào: thông số + số đo + thời gian → Đầu ra: máy nén, quạt dàn lạnh, cấp quạt dàn nóng.
 *
 *  Chung:   IDLE ──START──> STARTING (quạt chạy trước start_delay giây) ──> RUNNING (các giai đoạn)
 *           RUNNING ──STOP──> STOPPING (tắt máy nén, quạt chạy thêm fan_post) ──> IDLE
 *           bất kỳ ──lỗi──> FAULT (tắt máy nén ngay) ──RESET──> IDLE
 *
 *  TỰ ĐỘNG (mode = 0) – quạt dàn nóng cấp auto_fan (mặc định 3):
 *    AUTO_DRY  : máy nén chạy liên tục tới khi độ ẩm ≤ hum_set
 *    AUTO_HOLD : máy nén bật/tắt giữ nhiệt độ temp_set (bỏ qua độ ẩm)
 *    AUTO_COOL : hết thời gian sấy → tắt máy nén, quạt chạy tới khi nhiệt độ ≤ gd5_end_temp → kết thúc
 *                (thời gian sấy = 0 → chạy tới khi người dùng dừng)
 *
 *  THỦ CÔNG (mode = 1) – mỗi giai đoạn có cấp quạt riêng stage_fan[0..4], bỏ qua thời gian sấy:
 *    GD1 : quạt chạy trước start_delay → máy nén chạy liên tục tới khi nhiệt độ ≥ temp_set
 *    GD2 : giữ nhiệt độ, tới khi độ ẩm ≤ hum_set
 *    GD3 : giữ nhiệt độ trong gd3_min phút
 *    GD4 : giữ nhiệt độ trong gd4_min phút
 *    GD5 : tắt máy nén, quạt chạy tới khi nhiệt độ ≤ gd5_end_temp → kết thúc
 *
 *  Máy nén luôn có bảo vệ: nghỉ tối thiểu comp_min_off (chờ bật lại), chạy tối thiểu comp_min_on.
 *  Bảo vệ: quá nhiệt temp_max, áp cao/thấp, mất cảm biến nhiệt/áp.
 *  Quá nhiệt: tắt máy nén, quạt dàn lạnh + quạt dàn nóng cấp 5 chạy liên tục tới khi nhiệt độ
 *             ≤ temp_recover (mặc định 30 °C) → tự hết lỗi, về IDLE. Chưa nguội thì không reset được.
 */
#ifndef DRYER_CTRL_H
#define DRYER_CTRL_H

#include <stdbool.h>
#include <stdint.h>

#define DRYER_STAGES      5      /* số giai đoạn chế độ thủ công */
#define DRYER_FAN_LEVELS  5      /* số cấp quạt dàn nóng */

/* ---------- Thông số (được services/settings lưu Flash; kiểu float để dùng chung bảng menu) ---------- */
typedef struct {
    float temp_set;        /* °C   nhiệt độ sấy                                 */
    float temp_hyst;       /* °C   trễ nhiệt khi giữ nhiệt độ                   */
    float hum_set;         /* %RH  độ ẩm mục tiêu                               */
    float temp_recover;    /* °C   quá nhiệt: làm mát tới nhiệt độ này mới hết lỗi (thay hum_hyst cũ) */
    float temp_max;        /* °C   nhiệt độ bảo vệ → dừng máy                   */
    float p_high;          /* bar  ngắt áp cao                                  */
    float p_low;           /* bar  ngắt áp thấp (sau P_LOW_GRACE)               */
    float press_enable;    /* 0/1  bật bảo vệ áp suất                           */
    float comp_min_off;    /* s    chờ bật lại máy nén sau khi dừng             */
    float comp_min_on;     /* s    chạy tối thiểu máy nén                       */
    float start_delay;     /* s    quạt chạy trước khi bật máy nén              */
    float fan_post;        /* s    quạt chạy thêm sau khi dừng / lỗi            */
    float dry_time_h;      /* h    thời gian sấy (chỉ chế độ tự động), 0 = ∞    */
    float mode;            /* 0 = tự động, 1 = thủ công                         */
    float auto_fan;        /* 1..5 cấp quạt dàn nóng ở chế độ tự động           */
    float stage_fan[DRYER_STAGES];   /* 1..5 cấp quạt từng giai đoạn thủ công   */
    float gd3_min;         /* phút thời gian GD3                                */
    float gd4_min;         /* phút thời gian GD4                                */
    float end_temp;        /* °C   làm mát tới nhiệt độ này thì kết thúc (GD5)  */
} dryer_params_t;

typedef struct {
    bool temp_ok;  float temp;     /* °C  – nhiệt độ điều khiển (PT100) */
    bool hum_ok;   float hum;      /* %RH */
    bool press_ok; float press;    /* bar */
} dryer_input_t;

typedef struct {
    bool    comp;
    bool    fan_evap;
    uint8_t fan_level;             /* 0 = tắt, 1..5 = cấp quạt dàn nóng mong muốn */
} dryer_output_t;

typedef enum {
    DRYER_IDLE = 0,
    DRYER_STARTING,
    DRYER_RUNNING,
    DRYER_STOPPING,
    DRYER_FAULT,
} dryer_state_t;

typedef enum {
    PH_NONE = 0,
    PH_AUTO_DRY, PH_AUTO_HOLD, PH_AUTO_COOL,
    PH_GD1, PH_GD2, PH_GD3, PH_GD4, PH_GD5,
} dryer_phase_t;

typedef enum {
    DRYER_CMD_START = 0,
    DRYER_CMD_STOP,
    DRYER_CMD_TOGGLE,      /* IDLE → START, đang chạy → STOP */
    DRYER_CMD_RESET_FAULT,
} dryer_cmd_t;

/* Lỗi (dừng máy) */
#define DRYER_FAULT_TEMP_SENSOR   (1u << 0)
#define DRYER_FAULT_PRESS_SENSOR  (1u << 1)
#define DRYER_FAULT_PRESS_HIGH    (1u << 2)
#define DRYER_FAULT_PRESS_LOW     (1u << 3)
#define DRYER_FAULT_OVERTEMP      (1u << 4)
/* Cảnh báo (vẫn chạy) */
#define DRYER_WARN_HUM_SENSOR     (1u << 0)

typedef struct {
    dryer_state_t  state;
    dryer_phase_t  phase;
    bool           manual;         /* chu trình đang chạy là thủ công */
    uint16_t       faults;
    uint16_t       warnings;
    dryer_output_t out;
    bool           comp_demand;
    bool           target_reached; /* độ ẩm ≤ mục tiêu */
    bool           finished;       /* chu trình vừa kết thúc bình thường */
    uint32_t       comp_wait_s;    /* còn bao lâu được bật máy nén */
    uint32_t       run_s;          /* thời gian từ lúc bắt đầu */
    uint32_t       phase_s;        /* thời gian trong giai đoạn hiện tại */
    uint32_t       phase_left_s;   /* còn lại của giai đoạn có hẹn giờ (GD3/GD4), 0 nếu không */
} dryer_status_t;

/* Trạng thái nội bộ – app cấp phát, KHÔNG truy cập trực tiếp các field */
typedef struct {
    dryer_state_t state;
    dryer_phase_t phase;
    bool     manual, finished;
    uint32_t now, state_tick, phase_tick, run_start;
    uint32_t comp_on_tick, comp_off_tick;
    dryer_output_t out;
    bool     comp_demand, target_reached;
    uint8_t  pending_cmd;
    uint16_t faults, warnings;
    uint8_t  temp_bad_cnt, press_bad_cnt;
    uint32_t min_off_ms, phase_len_ms;
} dryer_ctrl_t;

void DryerCtrl_Init(dryer_ctrl_t *c, uint32_t now_ms);
void DryerCtrl_Command(dryer_ctrl_t *c, dryer_cmd_t cmd);
void DryerCtrl_Step(dryer_ctrl_t *c, const dryer_params_t *p, const dryer_input_t *in,
                    uint32_t now_ms, dryer_output_t *out);
void DryerCtrl_GetStatus(const dryer_ctrl_t *c, dryer_status_t *st);

const char *DryerCtrl_StateName(dryer_state_t s);
const char *DryerCtrl_PhaseName(dryer_phase_t ph);    /* "GĐ2 – hút ẩm", "Tự động – giữ nhiệt"… */
const char *DryerCtrl_FaultText(uint16_t faults);
float       DryerCtrl_RecoverTemp(const dryer_params_t *p);  /* nhiệt độ hết quá nhiệt (≤ temp_max - 5) */
bool        DryerCtrl_OvertempCooling(const dryer_ctrl_t *c); /* đang làm mát do quá nhiệt */     /* tên lỗi ưu tiên cao nhất, "" nếu không lỗi */
void DryerCtrl_DefaultParams(dryer_params_t *p);

#endif
