/**
 * @file    fan_speed.h
 * @brief   Quạt dàn nóng nhiều cấp kiểu CỘNG DỒN: cấp N = đóng relay 1..N (cấp 1 = 1 relay … cấp 5 = 5 relay).
 *          Tăng cấp: đóng thêm từng relay một, cách nhau FAN_SPEED_STEP_MS (giảm dòng khởi động).
 *          Giảm cấp / tắt: nhả ngay các relay thừa. Thuần C.
 *
 *  ⚠ Chỉ dùng khi mỗi relay cấp cho một tải riêng (nhiều quạt / nhiều nhánh song song).
 *    KHÔNG dùng cho motor nhiều đầu dây tốc độ – đóng 2 đầu dây cùng lúc sẽ chập cuộn dây.
 */
#ifndef FAN_SPEED_H
#define FAN_SPEED_H

#include <stdbool.h>
#include <stdint.h>

#define FAN_SPEED_STEP_MS   1000

typedef struct {
    uint8_t  actual;        /* số relay đang đóng = cấp thực tế (0 = tắt) */
    uint32_t step_tick;     /* lần đóng thêm relay gần nhất */
} fan_speed_t;

void    FanSpeed_Init(fan_speed_t *f, uint32_t now_ms);
/* target: cấp mong muốn (0..5). Trả về cấp thực tế (số relay đóng) lúc này. */
uint8_t FanSpeed_Step(fan_speed_t *f, uint8_t target, uint32_t now_ms);
/* relay cấp `stage` (1..5) có đóng ở cấp `level` không */
static inline bool FanSpeed_RelayOn(uint8_t level, uint8_t stage) { return stage >= 1 && stage <= level; }

#endif
