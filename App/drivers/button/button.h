/**
 * @file    button.h
 * @brief   Driver nút bấm tổng quát: chống dội, nhấn ngắn, giữ lâu, lặp.
 *          Bảng chân truyền vào lúc Init. Gọi Button_Scan() đều đặn mỗi BUTTON_SCAN_MS.
 *
 *  Nút có repeat = true : CLICK ngay khi nhấn, REPEAT khi giữ (dùng cho UP/DOWN)
 *  Nút có repeat = false: CLICK khi nhả (nhấn ngắn), LONG khi giữ >= BUTTON_LONG_MS
 */
#ifndef BUTTON_H
#define BUTTON_H

#include "stm32f1xx_hal.h"
#include <stdbool.h>
#include <stdint.h>

#ifndef BUTTON_SCAN_MS
#define BUTTON_SCAN_MS         10
#endif
#define BUTTON_DEBOUNCE_MS     30
#define BUTTON_LONG_MS         1000
#define BUTTON_REPEAT_START_MS 500
#define BUTTON_REPEAT_MS       150
#define BUTTON_MAX             8

typedef struct {
    GPIO_TypeDef *port;
    uint16_t      pin;
    GPIO_PinState active;     /* mức khi nhấn (pull-up + nút xuống GND: GPIO_PIN_RESET) */
    bool          repeat;
} button_hw_t;

typedef enum {
    BUTTON_EVT_CLICK = 0,
    BUTTON_EVT_LONG,
    BUTTON_EVT_REPEAT,
} button_evt_type_t;

typedef struct {
    uint8_t           id;     /* chỉ số trong bảng button_hw_t */
    button_evt_type_t type;
} button_evt_t;

void Button_Init(const button_hw_t *table, uint8_t count);
void Button_Scan(void);
bool Button_GetEvent(button_evt_t *evt);
bool Button_IsPressed(uint8_t id);

#endif
