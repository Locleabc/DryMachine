/**
 * @file    sensors.h
 * @brief   Dịch vụ cảm biến: đọc định kỳ, lọc, bù sai số, đánh giá hợp lệ.
 *          Tầng trên chỉ nhìn thấy sensors_data_t, không biết chip cụ thể.
 */
#ifndef SENSORS_H
#define SENSORS_H

#include "sht4x.h"
#include "ntc.h"
#include <stdbool.h>
#include <stdint.h>

#define SENSORS_FAST_MS       500    /* NTC buồng + NTC dàn lạnh */
#define SENSORS_SHT_MS        1000   /* SHT45 */
#define SENSORS_SHT_MAX_ERR   3      /* lỗi liên tiếp → báo mất cảm biến ẩm */
#define SENSORS_SHT_RECOVER   5      /* lỗi liên tiếp → khôi phục bus I2C + init lại */
#define SENSORS_NTC_ALPHA     0.3f   /* lọc EMA nhiệt độ NTC */
#define SENSORS_NTC_MAX_ERR   6      /* lần đọc lỗi liên tiếp (×500 ms = 3 s) mới báo mất NTC */
#define SENSORS_EVAP_COUNT    1      /* số NTC dàn lạnh */

/* Chỉ số NTC (mảng chẩn đoán ntc_*) */
enum { SENSORS_NTC_CHAMBER = 0, SENSORS_NTC_EVAP0, SENSORS_NTC_COUNT = SENSORS_NTC_EVAP0 + SENSORS_EVAP_COUNT };

typedef struct {
    const ntc_cfg_t          *chamber;                    /* NTC nhiệt độ buồng sấy (điều khiển) */
    const sht4x_cfg_t        *sht;
    const ntc_cfg_t          *evap[SENSORS_EVAP_COUNT];   /* NTC dàn lạnh (NULL = không có) */
    void (*bus_recover)(void);        /* khôi phục I2C bị treo (có thể NULL) */
} sensors_cfg_t;

typedef struct {
    float temp_offset;                /* °C cộng vào NTC buồng */
    float hum_offset;                 /* %RH cộng vào SHT45 */
    float evap_offset[SENSORS_EVAP_COUNT];   /* °C cộng vào NTC dàn lạnh */
} sensors_calib_t;

typedef struct {
    bool ok;
    float value;
} sensor_val_t;

typedef struct {
    sensor_val_t temp;       /* °C – NTC buồng sấy */
    sensor_val_t hum;        /* %RH – SHT45 */
    sensor_val_t hum_temp;   /* °C – nhiệt độ tại SHT45 (tham khảo) */
    sensor_val_t evap[SENSORS_EVAP_COUNT];   /* °C – NTC dàn lạnh */
    /* chẩn đoán từng NTC, chỉ số SENSORS_NTC_* */
    float        ntc_r[SENSORS_NTC_COUNT];      /* Ohm đo được (0 = hở/chập) */
    uint16_t     ntc_code[SENSORS_NTC_COUNT];   /* mã ADC 0..4095 lần đọc gần nhất */
    uint32_t     ntc_glitch[SENSORS_NTC_COUNT]; /* số lần đọc lỗi (kể cả đã lọc bỏ) */
    uint32_t     sht_errors;
    uint32_t     sht_serial;
    uint8_t      sht_status;   /* sht4x_status_t lần đọc gần nhất (chẩn đoán) */
    uint8_t      sht_addr;     /* địa chỉ I2C đang dùng */
    uint8_t      sht_family;   /* 0 = SHT4x, 1 = SHT3x */
} sensors_data_t;

void Sensors_Init(const sensors_cfg_t *cfg);
void Sensors_SetCalib(const sensors_calib_t *cal);
void Sensors_Process(uint32_t now_ms);          /* gọi liên tục trong vòng lặp */
const sensors_data_t *Sensors_Data(void);

#endif
