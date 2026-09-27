/**
 * @file    drv_pressure.h
 * @brief   Cảm biến áp suất analog 0.5–4.5 V qua phân áp vào ADC1.
 */
#ifndef DRV_PRESSURE_H
#define DRV_PRESSURE_H

#include <stdbool.h>

typedef struct {
    bool  ok;
    float bar;        /* áp suất (bar, gauge) */
    float volt;       /* điện áp tại chân cảm biến (đã nhân hệ số phân áp) */
} pressure_data_t;

void Pressure_Init(void);
bool Pressure_Read(pressure_data_t *out);

#endif
