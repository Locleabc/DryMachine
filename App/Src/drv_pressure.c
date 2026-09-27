/**
 * @file    drv_pressure.c
 */
#include "drv_pressure.h"
#include "app_config.h"

#define PRESS_OVERSAMPLE   8
#define PRESS_EMA_ALPHA    0.3f     /* lọc thông thấp: 0..1, nhỏ = mượt hơn */

static float s_volt_filt = -1.0f;

static uint16_t adc_read_once(void)
{
    uint16_t v = 0;
    HAL_ADC_Start(PRESSURE_ADC);
    if (HAL_ADC_PollForConversion(PRESSURE_ADC, 5) == HAL_OK) {
        v = (uint16_t)HAL_ADC_GetValue(PRESSURE_ADC);
    }
    HAL_ADC_Stop(PRESSURE_ADC);
    return v;
}

void Pressure_Init(void)
{
    HAL_ADCEx_Calibration_Start(PRESSURE_ADC);   /* hiệu chuẩn ADC F1 */
    s_volt_filt = -1.0f;
}

bool Pressure_Read(pressure_data_t *out)
{
    uint32_t sum = 0;
    for (int i = 0; i < PRESS_OVERSAMPLE; i++) sum += adc_read_once();

    float v = ((float)sum / PRESS_OVERSAMPLE) / PRESS_ADC_MAX * PRESS_ADC_VREF * PRESS_DIVIDER_RATIO;

    if (s_volt_filt < 0.0f) s_volt_filt = v;
    else s_volt_filt += PRESS_EMA_ALPHA * (v - s_volt_filt);

    out->volt = s_volt_filt;
    if (s_volt_filt < PRESS_V_FAULT_LOW || s_volt_filt > PRESS_V_FAULT_HIGH) {
        out->ok = false;
        out->bar = 0.0f;
        return false;
    }

    float bar = (s_volt_filt - PRESS_V_MIN) / (PRESS_V_MAX - PRESS_V_MIN)
                * (PRESS_P_MAX_BAR - PRESS_P_MIN_BAR) + PRESS_P_MIN_BAR;
    if (bar < PRESS_P_MIN_BAR) bar = PRESS_P_MIN_BAR;
    out->bar = bar;
    out->ok = true;
    return true;
}
