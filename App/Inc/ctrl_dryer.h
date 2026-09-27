/**
 * @file    ctrl_dryer.h
 * @brief   Máy trạng thái điều khiển máy sấy + bảo vệ máy nén.
 *
 *  IDLE ──Start──> STARTING (quạt dàn lạnh chạy trước) ──start_delay──> RUNNING
 *  RUNNING ──Stop / hết giờ──> STOPPING (tắt máy nén, quạt chạy thêm) ──fan_post──> IDLE
 *  bất kỳ ──lỗi──> FAULT (tắt máy nén ngay) ──ResetFault khi hết lỗi──> IDLE
 */
#ifndef CTRL_DRYER_H
#define CTRL_DRYER_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    CTRL_IDLE = 0,
    CTRL_STARTING,
    CTRL_RUNNING,
    CTRL_STOPPING,
    CTRL_FAULT,
} ctrl_state_t;

/* Lỗi (dừng máy) */
#define FAULT_TEMP_SENSOR   (1u << 0)
#define FAULT_PRESS_SENSOR  (1u << 1)
#define FAULT_PRESS_HIGH    (1u << 2)
#define FAULT_PRESS_LOW     (1u << 3)
#define FAULT_OVERTEMP      (1u << 4)
/* Cảnh báo (vẫn chạy) */
#define WARN_HUM_SENSOR     (1u << 0)   /* mất cảm biến ẩm → chỉ điều khiển theo nhiệt */

typedef struct {
    bool  temp_ok;  float temp;     /* °C  */
    bool  hum_ok;   float hum;      /* %RH */
    bool  press_ok; float press;    /* bar */
} ctrl_input_t;

typedef struct {
    ctrl_state_t state;
    uint16_t     faults;           /* lỗi đang giữ (latched) */
    uint16_t     warnings;
    bool         comp, fan_cond, fan_evap;
    bool         comp_demand;      /* logic muốn chạy máy nén */
    uint32_t     comp_wait_s;      /* còn bao lâu mới được bật máy nén */
    uint32_t     run_s;            /* thời gian đã sấy */
    bool         target_reached;   /* đã đạt độ ẩm mục tiêu */
} ctrl_status_t;

void Ctrl_Init(uint32_t now_ms);
void Ctrl_Start(void);
void Ctrl_Stop(void);
bool Ctrl_ResetFault(void);        /* true nếu reset được (hết lỗi) */
void Ctrl_Update(const ctrl_input_t *in, uint32_t now_ms);
void Ctrl_GetStatus(ctrl_status_t *st);
const char *Ctrl_StateName(ctrl_state_t s);
const char *Ctrl_FaultText(uint16_t faults);

#endif
