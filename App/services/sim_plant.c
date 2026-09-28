/**
 * @file    sim_plant.c
 */
#include "sim_plant.h"

/* ---- hệ số mô hình (đơn vị / giây) ---- */
#define HEAT_COMP      0.030f     /* °C/s máy nén chạy, quạt nóng tắt              */
#define HEAT_FAN_DUMP  0.004f     /* °C/s bớt đi mỗi cấp quạt dàn nóng khi MN chạy */
#define LOSS_BASE      0.0003f    /* 1/s  thất thoát nhiệt ra môi trường           */
#define LOSS_FAN       0.0002f    /* 1/s  thêm mỗi cấp quạt khi MN tắt (làm mát)   */
#define DRY_RATE       0.00060f   /* 1/s  tốc độ hút ẩm khi MN chạy (tỉ lệ theo RH) */
#define WET_RATE       0.002f     /* %/s  ẩm tăng lại khi MN tắt (vật sấy nhả ẩm)  */
#define HUM_MIN        5.0f

void SimPlant_Init(sim_plant_t *s, float temp, float hum)
{
    s->amb_temp = 30.0f;
    s->amb_hum  = 70.0f;
    s->temp  = temp;
    s->hum   = hum;
    s->press = 10.0f;
}

void SimPlant_Step(sim_plant_t *s, float dt, bool comp, uint8_t fan_level, bool fan_evap)
{
    if (dt <= 0.0f) return;
    if (dt > 60.0f) dt = 60.0f;                       /* giữ mô hình ổn định khi tua nhanh */
    float d = s->temp - s->amb_temp;

    if (comp) {
        float heat = HEAT_COMP - HEAT_FAN_DUMP * (float)fan_level;
        if (heat < 0.0f) heat = 0.0f;
        s->temp += (heat - LOSS_BASE * d) * dt;
        if (fan_evap) s->hum -= DRY_RATE * s->hum * dt;
        if (s->hum < HUM_MIN) s->hum = HUM_MIN;
    } else {
        s->temp -= (LOSS_BASE + LOSS_FAN * (float)fan_level) * d * dt;
        if (s->hum < s->amb_hum) s->hum += WET_RATE * dt;
    }

    /* áp suất đích: MN chạy ~ 15 bar + theo nhiệt độ buồng; tắt → cân bằng ~10 bar */
    float p_target = comp ? (15.0f + 0.1f * (s->temp - s->amb_temp) - 0.5f * (float)fan_level) : 10.0f;
    float k = dt / 5.0f; if (k > 1.0f) k = 1.0f;
    s->press += (p_target - s->press) * k;
}
