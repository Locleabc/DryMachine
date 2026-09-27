/**
 * @file    bsp_button.h
 * @brief   Quét 4 nút bấm (active low, pull-up): chống dội, nhấn, giữ lâu, lặp.
 *          Gọi Button_Scan() mỗi TASK_BUTTON_MS (10 ms).
 */
#ifndef BSP_BUTTON_H
#define BSP_BUTTON_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    BTN_UP = 0,
    BTN_DOWN,
    BTN_ENTER,
    BTN_EXIT,
    BTN_COUNT
} btn_id_t;

typedef enum {
    BTN_EVT_CLICK = 0,  /* vừa nhấn xuống (sau chống dội)       */
    BTN_EVT_LONG,       /* giữ >= BTN_LONG_MS (phát 1 lần)       */
    BTN_EVT_REPEAT,     /* giữ lâu, phát lặp (chỉ UP/DOWN)       */
} btn_evt_type_t;

typedef struct {
    btn_id_t       id;
    btn_evt_type_t type;
} btn_event_t;

void Button_Init(void);
void Button_Scan(void);
bool Button_GetEvent(btn_event_t *evt);
bool Button_IsPressed(btn_id_t id);

#endif
