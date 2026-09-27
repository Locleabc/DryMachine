/**
 * @file    drv_humidity.h
 * @brief   Cảm biến nhiệt-ẩm RS485 Modbus RTU (master, non-blocking).
 *          Mã cảm biến chưa chốt: chỉnh địa chỉ / thanh ghi trong app_config.h.
 *
 *  Cách dùng:  Humidity_Request() mỗi TASK_HUM_MS,  Humidity_Poll() liên tục trong loop.
 */
#ifndef DRV_HUMIDITY_H
#define DRV_HUMIDITY_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool     ok;          /* false khi lỗi liên tiếp >= HUM_MAX_ERRORS */
    float    rh;          /* %RH */
    float    temp_c;      /* nhiệt độ do cảm biến ẩm đo (tham khảo) */
    uint32_t err_count;   /* tổng số lỗi (debug) */
} humidity_data_t;

void Humidity_Init(void);
void Humidity_Request(void);
void Humidity_Poll(void);
void Humidity_Get(humidity_data_t *out);

#endif
