/**
 * @file    bsp_relay.h
 * @brief   Điều khiển relay trung gian (máy nén, quạt dàn nóng, quạt dàn lạnh).
 */
#ifndef BSP_RELAY_H
#define BSP_RELAY_H

#include <stdbool.h>

typedef enum {
    RELAY_COMP = 0,     /* Máy nén          */
    RELAY_FAN_COND,     /* Quạt dàn nóng    */
    RELAY_FAN_EVAP,     /* Quạt dàn lạnh    */
    RELAY_COUNT
} relay_id_t;

void Relay_Init(void);
void Relay_Set(relay_id_t id, bool on);
bool Relay_Get(relay_id_t id);
void Relay_AllOff(void);

#endif
