/**
 * @file    dryer_ctrl.h
 * @brief   Logic điều khiển máy sấy – THUẦN C: không HAL, không driver, không biến toàn cục.
 *          Đầu vào: thông số + số đo + thời gian → Đầu ra: máy nén, quạt dàn lạnh, cấp quạt dàn nóng.
 *
 *  Chung:   IDLE ──START──> STARTING (quạt chạy trước start_delay giây) ──> RUNNING (các giai đoạn)
 *           RUNNING ──STOP──> STOPPING (tắt máy nén, quạt chạy thêm fan_post) ──> IDLE
 *           bất kỳ ──lỗi──> FAULT (tắt máy nén ngay) ──RESET──> IDLE
 *
 *  CHU TRÌNH 5 GIAI ĐOẠN – mỗi giai đoạn có cấp quạt dàn nóng riêng stage_fan[0..4]:
 *    GD1 : quạt chạy trước start_delay → máy nén chạy liên tục tới khi nhiệt độ ≥ temp_set
 *    GD2 : giữ nhiệt độ, tới khi độ ẩm ≤ hum_set
 *    GD3 : giữ nhiệt độ; độ ẩm ≤ hum_gd3 LIÊN TỤC gd3_min phút → GD4
 *    GD4 : giữ nhiệt độ; độ ẩm ≤ hum_gd4 LIÊN TỤC gd4_min phút → GD5
 *          (ẩm vượt mức + HUM_HOLD_HYST thì đếm lại từ đầu; mất cảm biến ẩm → chỉ tính thời gian)
 *    GD5 : tắt máy nén, quạt chạy tới khi nhiệt độ ≤ end_temp → kết thúc
 *
 *  Máy nén luôn có bảo vệ: nghỉ tối thiểu comp_min_off (chờ bật lại), chạy tối thiểu comp_min_on.
 *  Bảo vệ: quá nhiệt temp_max, mất cảm biến nhiệt, chống đóng băng dàn lạnh.
 *  Chống đóng băng (evap_protect = 1): nhiệt độ dàn lạnh (thấp nhất của 2 NTC còn đọc được)
 *             ≤ evap_freeze liên tục evap_freeze_s giây khi máy nén chạy → XẢ ĐÁ: tắt máy nén
 *             (quạt vẫn chạy, giai đoạn vẫn tính giờ) tới khi dàn lạnh ≥ evap_resume → chạy tiếp.
 *             Không phải lỗi – không dừng máy. Mất NTC chỉ cảnh báo.
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
    float evap_freeze;     /* °C   dàn lạnh ≤ mức này (đủ evap_freeze_s) → xả đá   */
    float evap_resume;     /* °C   dàn lạnh ≥ mức này → hết xả đá                  */
    float evap_protect;    /* 0/1  bật chống đóng băng dàn lạnh                    */
    float comp_min_off;    /* s    chờ bật lại máy nén sau khi dừng             */
    float comp_min_on;     /* s    chạy tối thiểu máy nén                       */
    float start_delay;     /* s    quạt chạy trước khi bật máy nén              */
    float fan_post;        /* s    quạt chạy thêm sau khi dừng / lỗi            */
    float reserved0;       /* (thời gian sấy chế độ Tự động – đã bỏ, giữ chỗ Flash) */
    float hum_gd3;         /* %RH  GĐ3: độ ẩm phải ≤ mức này liên tục gd3_min   */
    float hum_gd4;         /* %RH  GĐ4: độ ẩm phải ≤ mức này liên tục gd4_min   */
    float stage_fan[DRYER_STAGES];   /* 1..5 cấp quạt từng giai đoạn thủ công   */
    float gd3_min;         /* phút GD3: thời gian giữ độ ẩm ≤ hum_gd3           */
    float gd4_min;         /* phút GD4: thời gian giữ độ ẩm ≤ hum_gd4           */
    float end_temp;        /* °C   làm mát tới nhiệt độ này thì kết thúc (GD5)  */
    float evap_freeze_s;   /* s    thời gian dưới evap_freeze mới xả đá            */
} dryer_params_t;

#define DRYER_EVAP_COUNT  2      /* số cảm biến dàn lạnh */

typedef struct {
    bool temp_ok;  float temp;     /* °C  – nhiệt độ điều khiển (PT100) */
    bool hum_ok;   float hum;      /* %RH */
    bool evap_ok[DRYER_EVAP_COUNT]; float evap[DRYER_EVAP_COUNT];   /* °C – NTC dàn lạnh */
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
/* bit 1..3: áp suất (đã bỏ) – giữ số để đọc đúng lịch sử lỗi cũ trong Flash */
#define DRYER_FAULT_OLD_PRESS     ((1u << 1) | (1u << 2) | (1u << 3))
#define DRYER_FAULT_OVERTEMP      (1u << 4)
/* Cảnh báo (vẫn chạy) */
#define DRYER_WARN_HUM_SENSOR     (1u << 0)
#define DRYER_WARN_EVAP_SENSOR    (1u << 1)   /* mất ≥ 1 NTC dàn lạnh (khi bật chống đóng băng) */
#define DRYER_WARN_DEFROST        (1u << 2)   /* đang xả đá: máy nén tạm tắt */

typedef struct {
    dryer_state_t  state;
    dryer_phase_t  phase;
    uint16_t       faults;
    uint16_t       warnings;
    dryer_output_t out;
    bool           comp_demand;
    bool           target_reached; /* độ ẩm ≤ mục tiêu */
    bool           finished;       /* chu trình vừa kết thúc bình thường */
    uint32_t       comp_wait_s;    /* còn bao lâu được bật máy nén */
    uint32_t       run_s;          /* thời gian từ lúc bắt đầu */
    uint32_t       phase_s;        /* thời gian trong giai đoạn hiện tại */
    uint32_t       phase_left_s;   /* GD3/GD4: thời gian giữ ẩm còn lại (đang đếm) hoặc cả khoảng (chưa đạt ẩm) */
    bool           hold_counting;  /* GD3/GD4: độ ẩm đang đạt mức → đang đếm */
    bool           defrost;        /* đang xả đá dàn lạnh */
    bool           evap_ok;        /* có ít nhất 1 NTC dàn lạnh đọc được */
    float          evap_min;       /* °C nhiệt độ dàn lạnh dùng cho chống đóng băng */
    uint16_t       defrost_count;  /* số lần xả đá từ lúc bật nguồn */
} dryer_status_t;

/* Trạng thái nội bộ – app cấp phát, KHÔNG truy cập trực tiếp các field */
typedef struct {
    dryer_state_t state;
    dryer_phase_t phase;
    bool     finished, hold_counting;
    uint32_t hold_tick;
    uint32_t now, state_tick, phase_tick, run_start;
    uint32_t comp_on_tick, comp_off_tick;
    dryer_output_t out;
    bool     comp_demand, target_reached;
    uint8_t  pending_cmd;
    uint16_t faults, warnings;
    uint8_t  temp_bad_cnt;
    bool     defrost, evap_ok, freezing;
    float    evap_min;
    uint32_t freeze_tick;
    uint16_t defrost_count;
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
