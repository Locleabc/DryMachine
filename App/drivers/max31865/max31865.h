/**
 * @file    max31865.h
 * @brief   Driver MAX31865 (PT100/PT1000 qua SPI mode 1). Nhiều instance được.
 */
#ifndef MAX31865_H
#define MAX31865_H

#include "stm32f1xx_hal.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    SPI_HandleTypeDef *hspi;
    GPIO_TypeDef      *cs_port;
    uint16_t           cs_pin;
    float              rref;        /* Ohm – điện trở tham chiếu (module PT100: 430) */
    float              r0;          /* 100 (PT100) hoặc 1000 (PT1000) */
    uint8_t            wires;       /* 2, 3 hoặc 4 */
    bool               filter_50hz;
} max31865_cfg_t;

typedef struct {
    const max31865_cfg_t *cfg;
} max31865_t;

typedef struct {
    bool    ok;
    float   temp_c;
    float   r_ohm;
    uint8_t fault;          /* thanh ghi lỗi (0xFF = không đọc được chip) */
    uint16_t raw;           /* mã ADC 15 bit (chẩn đoán) */
} max31865_result_t;

void MAX31865_Init(max31865_t *dev, const max31865_cfg_t *cfg);
bool MAX31865_Read(max31865_t *dev, max31865_result_t *out);
float MAX31865_ResistanceToTemp(float r_ohm, float r0);    /* hàm thuần, dùng lại được */

#endif
