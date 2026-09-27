/**
 * @file    press_analog.c
 */
#include "press_analog.h"

static uint16_t adc_once(ADC_HandleTypeDef *h)
{
    uint16_t v = 0;
    HAL_ADC_Start(h);
    if (HAL_ADC_PollForConversion(h, 5) == HAL_OK) v = (uint16_t)HAL_ADC_GetValue(h);
    HAL_ADC_Stop(h);
    return v;
}

void PressAnalog_Init(press_analog_t *dev, const press_analog_cfg_t *cfg)
{
    dev->cfg = cfg;
    HAL_ADCEx_Calibration_Start(cfg->hadc);      /* hiệu chuẩn ADC dòng F1 */
}

float PressAnalog_ReadVolt(press_analog_t *dev)
{
    const press_analog_cfg_t *c = dev->cfg;
    uint8_t n = c->oversample ? c->oversample : 1;
    uint32_t sum = 0;
    for (uint8_t i = 0; i < n; i++) sum += adc_once(c->hadc);
    return ((float)sum / n) / c->adc_max * c->vref * c->divider;
}

bool PressAnalog_VoltToBar(const press_analog_cfg_t *c, float v, float *bar)
{
    if (v < c->v_fault_low || v > c->v_fault_high) {
        *bar = 0.0f;
        return false;
    }
    float p = (v - c->v_min) / (c->v_max - c->v_min) * (c->p_max - c->p_min) + c->p_min;
    *bar = (p < c->p_min) ? c->p_min : p;
    return true;
}
