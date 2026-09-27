/**
 * @file    button.c
 */
#include "button.h"
#include <string.h>

#define TICKS(ms)      ((uint16_t)((ms) / BUTTON_SCAN_MS))
#define QUEUE_SIZE     8

typedef struct {
    uint8_t  cnt;
    bool     stable;
    uint16_t hold;
    bool     long_sent;
} btn_state_t;

static const button_hw_t *s_hw;
static uint8_t     s_count;
static btn_state_t s_btn[BUTTON_MAX];
static button_evt_t s_queue[QUEUE_SIZE];
static uint8_t     s_head, s_tail;

static void push(uint8_t id, button_evt_type_t type)
{
    uint8_t next = (uint8_t)((s_head + 1) % QUEUE_SIZE);
    if (next == s_tail) return;               /* hàng đợi đầy */
    s_queue[s_head].id = id;
    s_queue[s_head].type = type;
    s_head = next;
}

void Button_Init(const button_hw_t *table, uint8_t count)
{
    s_hw = table;
    s_count = (count > BUTTON_MAX) ? BUTTON_MAX : count;
    memset(s_btn, 0, sizeof(s_btn));
    s_head = s_tail = 0;
}

void Button_Scan(void)
{
    for (uint8_t i = 0; i < s_count; i++) {
        btn_state_t *b = &s_btn[i];
        const button_hw_t *h = &s_hw[i];
        bool raw = (HAL_GPIO_ReadPin(h->port, h->pin) == h->active);

        if (raw != b->stable) {
            if (++b->cnt >= TICKS(BUTTON_DEBOUNCE_MS)) {
                b->cnt = 0;
                b->stable = raw;
                if (h->repeat) {
                    if (raw) push(i, BUTTON_EVT_CLICK);
                } else if (!raw && !b->long_sent) {
                    push(i, BUTTON_EVT_CLICK);
                }
                b->hold = 0;
                b->long_sent = false;
            }
        } else {
            b->cnt = 0;
        }

        if (b->stable) {
            if (b->hold < 0xFFFF) b->hold++;
            if (h->repeat) {
                if (b->hold >= TICKS(BUTTON_REPEAT_START_MS) &&
                    ((b->hold - TICKS(BUTTON_REPEAT_START_MS)) % TICKS(BUTTON_REPEAT_MS)) == 0) {
                    push(i, BUTTON_EVT_REPEAT);
                }
            } else if (!b->long_sent && b->hold >= TICKS(BUTTON_LONG_MS)) {
                b->long_sent = true;
                push(i, BUTTON_EVT_LONG);
            }
        }
    }
}

bool Button_GetEvent(button_evt_t *evt)
{
    if (s_tail == s_head) return false;
    *evt = s_queue[s_tail];
    s_tail = (uint8_t)((s_tail + 1) % QUEUE_SIZE);
    return true;
}

bool Button_IsPressed(uint8_t id)
{
    return (id < s_count) && s_btn[id].stable;
}
