/**
 * @file    fan_speed.c
 */
#include "fan_speed.h"

void FanSpeed_Init(fan_speed_t *f, uint32_t now_ms)
{
    f->actual = 0;
    f->off_tick = now_ms - FAN_SPEED_DEADTIME_MS;   /* được bật ngay lần đầu */
}

uint8_t FanSpeed_Step(fan_speed_t *f, uint8_t target, uint32_t now_ms)
{
    if (target == f->actual) return f->actual;
    if (f->actual != 0) {                     /* đang chạy cấp khác → tắt trước */
        f->actual = 0;
        f->off_tick = now_ms;
        return 0;
    }
    if (now_ms - f->off_tick >= FAN_SPEED_DEADTIME_MS) f->actual = target;
    return f->actual;
}
