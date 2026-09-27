/**
 * @file    drv_max31865.h
 * @brief   Đọc PT100 qua MAX31865 (SPI mode 1, chế độ chuyển đổi liên tục).
 */
#ifndef DRV_MAX31865_H
#define DRV_MAX31865_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool    ok;          /* true = giá trị hợp lệ               */
    float   temp_c;      /* nhiệt độ (°C)                       */
    float   r_ohm;       /* điện trở PT100 đo được              */
    uint8_t fault;       /* thanh ghi lỗi MAX31865 (0 = không)  */
} max31865_data_t;

void MAX31865_Init(void);
bool MAX31865_Read(max31865_data_t *out);

#endif
