/**
 * @file    relay.h
 * @brief   Driver relay/đầu ra số tổng quát. Bảng chân được truyền vào lúc Init,
 *          driver không biết relay nào là máy nén hay quạt.
 */
#ifndef RELAY_H
#define RELAY_H

#include "stm32f1xx_hal.h"
#include <stdbool.h>
#include <stdint.h>

#define RELAY_MAX   8

typedef struct {
    GPIO_TypeDef *port;
    uint16_t      pin;
    GPIO_PinState active;     /* mức kích: GPIO_PIN_SET hoặc GPIO_PIN_RESET */
} relay_hw_t;

void Relay_Init(const relay_hw_t *table, uint8_t count);   /* tắt tất cả */
void Relay_Set(uint8_t id, bool on);
bool Relay_Get(uint8_t id);
void Relay_AllOff(void);

#endif
