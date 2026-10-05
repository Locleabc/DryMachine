/**
 * @file    fan_speed.c
 */
#include "fan_speed.h"

void FanSpeed_Init(fan_speed_t *f, uint32_t now_ms)
{
    f->actual = 0;
    f->step_tick = now_ms - FAN_SPEED_STEP_MS;      /* relay đầu tiên được đóng ngay */
}

uint8_t FanSpeed_Step(fan_speed_t *f, uint8_t target, uint32_t now_ms)
{
    if (target < f->actual) {                       /* giảm / tắt: nhả ngay */
        f->actual = target;
    } else if (target > f->actual && now_ms - f->step_tick >= FAN_SPEED_STEP_MS) {
        f->actual++;                                /* tăng: thêm từng relay một */
        f->step_tick = now_ms;
    }
    return f->actual;
}
