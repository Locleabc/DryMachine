/**
 * @file    dryer_ctrl.h
 * @brief   Logic điều khiển máy sấy – THUẦN C: không HAL, không driver, không biến toàn cục.
 *          Đầu vào: thông số + số đo + thời gian → Đầu ra: trạng thái 3 relay.
 *
 *  IDLE ──START──> STARTING (quạt dàn lạnh chạy trước) ──start_delay──> RUNNING
 *  RUNNING ──STOP / hết giờ──> STOPPING (tắt máy nén, quạt chạy thêm) ──fan_post──> IDLE
 *  bất kỳ ──lỗi──> FAULT (tắt máy nén ngay) ──RESET──> IDLE
 */
#ifndef DRYER_CTRL_H
#define DRYER_CTRL_H

#include <stdbool.h>
#include <stdint.h>

/* ---------- Thông số (được services/settings lưu Flash) ---------- */
typedef struct {
    float temp_set;        /* °C   nhiệt độ sấy                               */
    float temp_hyst;       /* °C   trễ nhiệt                                  */
    float hum_set;         /* %RH  độ ẩm mục tiêu                             */
    float hum_hyst;        /* %RH  trễ ẩm                                     */
    float temp_max;        /* °C   ngắt quá nhiệt                             */
    float p_high;          /* bar  ngắt áp cao                                */
    float p_low;           /* bar  ngắt áp thấp (sau P_LOW_GRACE)             */
    float press_enable;    /* 0/1  bật bảo vệ áp suất                         */
    float comp_min_off;    /* s    nghỉ tối thiểu máy nén                     */
    float comp_min_on;     /* s    chạy tối thiểu máy nén                     */
    float start_delay;     /* s    quạt dàn lạnh chạy trước máy nén           */
    float fan_post;        /* s    quạt chạy thêm sau khi dừng                */
    float dry_time_h;      /* h    thời gian sấy, 0 = không giới hạn          */
    float cond_fan_mode;   /* 0 = theo máy nén, 1 = theo nhiệt độ (xả nhiệt)  */
} dryer_params_t;

typedef struct {
    bool temp_ok;  float temp;     /* °C  – nhiệt độ điều khiển (PT100) */
    bool hum_ok;   float hum;      /* %RH */
    bool press_ok; float press;    /* bar */
} dryer_input_t;

typedef struct {
    bool comp;
    bool fan_cond;
    bool fan_evap;
} dryer_output_t;

typedef enum {
    DRYER_IDLE = 0,
    DRYER_STARTING,
    DRYER_RUNNING,
    DRYER_STOPPING,
    DRYER_FAULT,
} dryer_state_t;

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
    dryer_state_t state;
    uint16_t      faults;
    uint16_t      warnings;
    dryer_output_t out;
    bool          comp_demand;
    bool          target_reached;
    uint32_t      comp_wait_s;     /* còn bao lâu được bật máy nén */
    uint32_t      run_s;           /* thời gian đã sấy */
} dryer_status_t;

/* Trạng thái nội bộ – app cấp phát, KHÔNG truy cập trực tiếp các field */
typedef struct {
    dryer_state_t state;
    uint32_t now, state_tick, run_start;
    uint32_t comp_on_tick, comp_off_tick;
    dryer_output_t out;
    bool     comp_demand, target_reached;
    uint8_t  pending_cmd;          /* bit = dryer_cmd_t */
    uint16_t faults, warnings;
    uint8_t  temp_bad_cnt, press_bad_cnt;
    uint32_t min_off_ms;           /* chép từ params để tính comp_wait_s */
} dryer_ctrl_t;

void DryerCtrl_Init(dryer_ctrl_t *c, uint32_t now_ms);
void DryerCtrl_Command(dryer_ctrl_t *c, dryer_cmd_t cmd);
void DryerCtrl_Step(dryer_ctrl_t *c, const dryer_params_t *p, const dryer_input_t *in,
                    uint32_t now_ms, dryer_output_t *out);
void DryerCtrl_GetStatus(const dryer_ctrl_t *c, dryer_status_t *st);

const char *DryerCtrl_StateName(dryer_state_t s);
const char *DryerCtrl_FaultText(uint16_t faults);    /* tên lỗi ưu tiên cao nhất, "" nếu không lỗi */
void DryerCtrl_DefaultParams(dryer_params_t *p);

#endif
