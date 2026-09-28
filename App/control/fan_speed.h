/**
 * @file    fan_speed.h
 * @brief   Khoá liên động quạt nhiều cấp (mỗi cấp 1 relay, chỉ 1 relay được đóng).
 *          Khi đổi cấp: tắt hết → chờ FAN_SPEED_DEADTIME_MS → đóng cấp mới
 *          (tránh 2 cuộn dây tốc độ của motor cùng có điện). Thuần C.
 */
#ifndef FAN_SPEED_H
#define FAN_SPEED_H

#include <stdint.h>

#define FAN_SPEED_DEADTIME_MS   1000

typedef struct {
    uint8_t  actual;        /* cấp đang đóng relay (0 = tắt) */
    uint32_t off_tick;      /* thời điểm tắt gần nhất */
} fan_speed_t;

void    FanSpeed_Init(fan_speed_t *f, uint32_t now_ms);
/* target: cấp mong muốn (0..5). Trả về cấp cần đóng relay ngay lúc này. */
uint8_t FanSpeed_Step(fan_speed_t *f, uint8_t target, uint32_t now_ms);

#endif
