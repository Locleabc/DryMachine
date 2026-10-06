/**
 * @file    ntc.h
 * @brief   Nhiệt điện trở NTC qua ADC, mạch phân áp tỉ lệ (không phụ thuộc Vref):
 *
 *              3.3V ── R_fixed (10k 1%) ──┬── chân ADC ──┬── 100 nF ── GND
 *                                         └── NTC ───────┴── GND
 *
 *          R_ntc = R_fixed · code / (code_max − code).  T = 1 / (1/T25 + ln(R/R25)/B) − 273.15
 *          Nhiều instance trên cùng ADC: mỗi lần đọc tự chọn kênh (ADC chế độ 1 kênh, phần mềm kích).
 */
#ifndef NTC_H
#define NTC_H

#include "stm32f1xx_hal.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    ADC_HandleTypeDef *hadc;
    uint32_t channel;          /* ADC_CHANNEL_x */
    float    r_fixed;          /* Ohm – điện trở phân áp phía 3.3V */
    float    r25;              /* Ohm – NTC ở 25 °C */
    float    adc_max;          /* 4095 */
    uint16_t code_min;         /* dưới mức này = chập NTC */
    uint16_t code_max;         /* trên mức này = hở / chưa cắm NTC */
    uint8_t  oversample;       /* số mẫu trung bình */
} ntc_cfg_t;

typedef struct {
    const ntc_cfg_t *cfg;
} ntc_t;

void     NTC_Init(ntc_t *dev, const ntc_cfg_t *cfg);
uint16_t NTC_ReadCode(ntc_t *dev);                                   /* mã ADC trung bình */
/* hàm thuần: mã ADC → điện trở / nhiệt độ. false = hở hoặc chập */
bool     NTC_CodeToOhm(const ntc_cfg_t *cfg, uint16_t code, float *ohm);
float    NTC_OhmToTemp(float ohm, float r25, float beta);

#endif
