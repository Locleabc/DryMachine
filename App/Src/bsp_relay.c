/**
 * @file    bsp_relay.c
 */
#include "bsp_relay.h"
#include "app_config.h"

typedef struct {
    GPIO_TypeDef *port;
    uint16_t      pin;
} relay_hw_t;

static const relay_hw_t s_hw[RELAY_COUNT] = {
    [RELAY_COMP]     = { RLY_COMP_GPIO_Port,     RLY_COMP_Pin     },
    [RELAY_FAN_COND] = { RLY_FAN_COND_GPIO_Port, RLY_FAN_COND_Pin },
    [RELAY_FAN_EVAP] = { RLY_FAN_EVAP_GPIO_Port, RLY_FAN_EVAP_Pin },
};

static bool s_state[RELAY_COUNT];

#define RELAY_INACTIVE_LEVEL  ((RELAY_ACTIVE_LEVEL == GPIO_PIN_SET) ? GPIO_PIN_RESET : GPIO_PIN_SET)

void Relay_Init(void)
{
    Relay_AllOff();
}

void Relay_Set(relay_id_t id, bool on)
{
    if (id >= RELAY_COUNT) return;
    HAL_GPIO_WritePin(s_hw[id].port, s_hw[id].pin, on ? RELAY_ACTIVE_LEVEL : RELAY_INACTIVE_LEVEL);
    s_state[id] = on;
}

bool Relay_Get(relay_id_t id)
{
    return (id < RELAY_COUNT) ? s_state[id] : false;
}

void Relay_AllOff(void)
{
    for (int i = 0; i < RELAY_COUNT; i++) {
        Relay_Set((relay_id_t)i, false);
    }
}
