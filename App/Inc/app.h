/**
 * @file    app.h
 * @brief   Điểm vào ứng dụng. Gọi App_Init() 1 lần sau MX_xxx_Init(), App_Loop() trong while(1).
 */
#ifndef APP_H
#define APP_H

#include "drv_max31865.h"
#include "drv_humidity.h"
#include "drv_pressure.h"

typedef struct {
    max31865_data_t temp;
    humidity_data_t hum;
    pressure_data_t press;
} app_meas_t;

void App_Init(void);
void App_Loop(void);
void App_Log(const char *fmt, ...);

#endif
