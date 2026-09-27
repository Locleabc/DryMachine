/**
 * @file    press_analog.h
 * @brief   Cảm biến áp suất đầu ra điện áp tuyến tính (vd 0.5–4.5 V) qua ADC.
 *          Driver chỉ đọc + quy đổi; lọc số nằm ở tầng services/sensors.
 */
#ifndef PRESS_ANALOG_H
#define PRESS_ANALOG_H

#include "stm32f1xx_hal.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    ADC_HandleTypeDef *hadc;
    float   vref;            /* 3.3 */
    float   adc_max;         /* 4095 */
    float   divider;         /* Vsensor = Vadc * divider */
    float   v_min, v_max;    /* điện áp tại p_min / p_max */
    float   p_min, p_max;    /* bar */
    float   v_fault_low;     /* dưới mức này = đứt dây */
    float   v_fault_high;    /* trên mức này = chập */
    uint8_t oversample;      /* số mẫu lấy trung bình */
} press_analog_cfg_t;

typedef struct {
    const press_analog_cfg_t *cfg;
} press_analog_t;

void  PressAnalog_Init(press_analog_t *dev, const press_analog_cfg_t *cfg);
float PressAnalog_ReadVolt(press_analog_t *dev);                    /* V tại chân cảm biến */
bool  PressAnalog_VoltToBar(const press_analog_cfg_t *cfg, float v, float *bar);  /* hàm thuần */

#endif
