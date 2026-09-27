/**
 * @file    relay.c
 */
#include "relay.h"

static const relay_hw_t *s_hw;
static uint8_t s_count;
static uint8_t s_state;          /* bit i = relay i đang ON */

void Relay_Init(const relay_hw_t *table, uint8_t count)
{
    s_hw = table;
    s_count = (count > RELAY_MAX) ? RELAY_MAX : count;
    Relay_AllOff();
}

void Relay_Set(uint8_t id, bool on)
{
    if (!s_hw || id >= s_count) return;
    GPIO_PinState inactive = (s_hw[id].active == GPIO_PIN_SET) ? GPIO_PIN_RESET : GPIO_PIN_SET;
    HAL_GPIO_WritePin(s_hw[id].port, s_hw[id].pin, on ? s_hw[id].active : inactive);
    if (on) s_state |= (uint8_t)(1u << id);
    else    s_state &= (uint8_t)~(1u << id);
}

bool Relay_Get(uint8_t id)
{
    return (id < s_count) && (s_state & (1u << id));
}

void Relay_AllOff(void)
{
    for (uint8_t i = 0; i < s_count; i++) Relay_Set(i, false);
}
