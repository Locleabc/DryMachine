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
    s->amb_temp = 28.0f;         /* < 30 °C để thử được "quá nhiệt → nguội tới 30 °C" */
    s->amb_hum  = 70.0f;
    s->temp  = temp;
    s->hum   = hum;
    s->evap  = temp;
    s->icing = false;
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

    /* dàn lạnh: MN chạy → 2 °C (đóng băng −8 °C), tắt → về nhiệt độ buồng; hằng số thời gian 30 s */
    float e_target = comp ? (s->icing ? -8.0f : 2.0f) : s->temp;
    float k = dt / 30.0f; if (k > 1.0f) k = 1.0f;
    s->evap += (e_target - s->evap) * k;
}
