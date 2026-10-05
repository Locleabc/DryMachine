/**
 * @file    sensors.h
 * @brief   Dịch vụ cảm biến: đọc định kỳ, lọc, bù sai số, đánh giá hợp lệ.
 *          Tầng trên chỉ nhìn thấy sensors_data_t, không biết chip cụ thể.
 */
#ifndef SENSORS_H
#define SENSORS_H

#include "max31865.h"
#include "sht4x.h"
#include "press_analog.h"
#include <stdbool.h>
#include <stdint.h>

#define SENSORS_FAST_MS       500    /* PT100 + áp suất */
#define SENSORS_SHT_MS        1000   /* SHT45 */
#define SENSORS_PT100_START_MS 5000  /* sau khi bật nguồn chờ 5 s mới khởi tạo + đọc MAX31865 (nguồn/bias ổn định) */
#define SENSORS_SHT_MAX_ERR   3      /* lỗi liên tiếp → báo mất cảm biến ẩm */
#define SENSORS_SHT_RECOVER   5      /* lỗi liên tiếp → khôi phục bus I2C + init lại */
#define SENSORS_PRESS_ALPHA   0.3f   /* lọc EMA áp suất */

typedef struct {
    const max31865_cfg_t     *pt100;
    const sht4x_cfg_t        *sht;
    const press_analog_cfg_t *press;
    void (*bus_recover)(void);        /* khôi phục I2C bị treo (có thể NULL) */
} sensors_cfg_t;

typedef struct {
    float temp_offset;                /* °C cộng vào PT100 */
    float hum_offset;                 /* %RH cộng vào SHT45 */
} sensors_calib_t;

typedef struct {
    bool ok;
    float value;
} sensor_val_t;

typedef struct {
    sensor_val_t temp;       /* °C – PT100 */
    sensor_val_t hum;        /* %RH – SHT45 */
    sensor_val_t hum_temp;   /* °C – nhiệt độ tại SHT45 (tham khảo) */
    sensor_val_t press;      /* bar */
    float        press_volt;
    uint8_t      pt100_fault;
    uint8_t      pt100_cfg;    /* thanh ghi cấu hình MAX31865 đọc lại (chẩn đoán) */
    uint16_t     pt100_reinit; /* số lần tự ghi lại cấu hình (chip mất cấu hình) */
    bool         pt100_ready;  /* false trong SENSORS_PT100_START_MS đầu: chưa đọc, không tính là lỗi */
    float        pt100_r;      /* Ohm đo được lần gần nhất (chẩn đoán, 0 = không có) */
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
