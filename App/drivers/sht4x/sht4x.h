/**
 * @file    sht4x.h
 * @brief   Driver Sensirion SHT40/SHT41/SHT45 (I2C). Đo không chặn:
 *          SHT4X_StartMeasure() → chờ SHT4X_MeasureTimeMs() → SHT4X_ReadResult().
 */
#ifndef SHT4X_H
#define SHT4X_H

#include "stm32f1xx_hal.h"
#include <stdbool.h>
#include <stdint.h>

#define SHT4X_ADDR_A   0x44     /* SHT45-AD1B (phổ biến) */
#define SHT4X_ADDR_B   0x45     /* SHT45-BD1B */

typedef enum {
    SHT4X_PREC_HIGH   = 0xFD,   /* max 8.3 ms, lặp lại 0.08 %RH */
    SHT4X_PREC_MEDIUM = 0xF6,   /* max 4.5 ms */
    SHT4X_PREC_LOW    = 0xE0,   /* max 1.6 ms */
} sht4x_prec_t;

/* Bộ sấy tích hợp: dùng khi độ ẩm cao lâu ngày để chống trôi (creep) / đọng sương.
 * Sau lệnh heater, cảm biến trả kết quả đo ở cuối chu kỳ sấy. Không dùng liên tục (duty ≤ 10%). */
typedef enum {
    SHT4X_HEAT_200MW_1S    = 0x39,
    SHT4X_HEAT_200MW_100MS = 0x32,
    SHT4X_HEAT_110MW_1S    = 0x2F,
    SHT4X_HEAT_110MW_100MS = 0x24,
    SHT4X_HEAT_20MW_1S     = 0x1E,
    SHT4X_HEAT_20MW_100MS  = 0x15,
} sht4x_heater_t;

typedef enum {
    SHT4X_OK = 0,
    SHT4X_ERR_BUS,        /* lỗi I2C khác (timeout, mất trọng tài) */
    SHT4X_ERR_CRC,        /* dữ liệu sai CRC – nhiễu / dây dài */
    SHT4X_ERR_NACK,       /* không có thiết bị trả lời ở địa chỉ – dây, nguồn, sai địa chỉ */
    SHT4X_ERR_BUSY,       /* bus bận: SDA/SCL bị giữ mức thấp, thiếu pull-up */
} sht4x_status_t;

typedef struct {
    I2C_HandleTypeDef *hi2c;
    uint8_t            addr;       /* 7 bit: SHT4X_ADDR_A / SHT4X_ADDR_B */
    sht4x_prec_t       precision;
} sht4x_cfg_t;

typedef struct {
    const sht4x_cfg_t *cfg;
    uint8_t            addr;       /* địa chỉ đang dùng (Init tự thử 0x44 rồi 0x45) */
    uint32_t           serial;
    uint32_t           busy_ms;    /* thời gian chờ của lệnh vừa gửi */
} sht4x_t;

typedef struct {
    float temp_c;
    float rh;                      /* đã kẹp 0..100 %RH */
} sht4x_result_t;

sht4x_status_t SHT4X_Init(sht4x_t *dev, const sht4x_cfg_t *cfg);     /* soft reset + đọc serial; thử cả 0x44/0x45 */
sht4x_status_t SHT4X_StartMeasure(sht4x_t *dev);
sht4x_status_t SHT4X_StartHeater(sht4x_t *dev, sht4x_heater_t heat);
uint32_t       SHT4X_BusyTimeMs(const sht4x_t *dev);                 /* chờ bao lâu trước khi đọc */
sht4x_status_t SHT4X_ReadResult(sht4x_t *dev, sht4x_result_t *out);

#endif
