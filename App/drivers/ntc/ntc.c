/**
 * @file    ntc.c
 */
#include "ntc.h"
#include <math.h>

#define T25_K  298.15f

static uint16_t adc_once(ADC_HandleTypeDef *h)
{
    uint16_t v = 0;
    HAL_ADC_Start(h);
    if (HAL_ADC_PollForConversion(h, 5) == HAL_OK) v = (uint16_t)HAL_ADC_GetValue(h);
    HAL_ADC_Stop(h);
    return v;
}

void NTC_Init(ntc_t *dev, const ntc_cfg_t *cfg)
{
    dev->cfg = cfg;
    HAL_ADCEx_Calibration_Start(cfg->hadc);      /* hiệu chuẩn ADC dòng F1 (gọi lại không hại) */
}

uint16_t NTC_ReadCode(ntc_t *dev)
{
    const ntc_cfg_t *c = dev->cfg;
    ADC_ChannelConfTypeDef ch = {0};
    ch.Channel      = c->channel;
    ch.Rank         = ADC_REGULAR_RANK_1;
    ch.SamplingTime = ADC_SAMPLETIME_239CYCLES_5;     /* nguồn trở kháng cao (~10 kΩ) */
    HAL_ADC_ConfigChannel(c->hadc, &ch);

    uint8_t n = c->oversample ? c->oversample : 1;
    uint32_t sum = 0;
    (void)adc_once(c->hadc);                          /* bỏ mẫu đầu sau khi đổi kênh */
    for (uint8_t i = 0; i < n; i++) sum += adc_once(c->hadc);
    return (uint16_t)(sum / n);
}

bool NTC_CodeToOhm(const ntc_cfg_t *c, uint16_t code, float *ohm)
{
    if (code < c->code_min || code > c->code_max || (float)code >= c->adc_max) {
        *ohm = 0.0f;
        return false;
    }
    *ohm = c->r_fixed * (float)code / (c->adc_max - (float)code);
    return true;
}

float NTC_OhmToTemp(float ohm, float r25, float beta)
{
    if (ohm <= 0.0f || r25 <= 0.0f || beta <= 0.0f) return -273.15f;
    return 1.0f / (1.0f / T25_K + logf(ohm / r25) / beta) - 273.15f;
}
