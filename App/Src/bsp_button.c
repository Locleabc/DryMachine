/**
 * @file    bsp_button.c
 */
#include "bsp_button.h"
#include "app_config.h"

#define BTN_DEBOUNCE_TICKS   3      /* 3 x 10 ms  */
#define BTN_LONG_TICKS       100    /* 1000 ms    */
#define BTN_REPEAT_START     50     /* 500 ms     */
#define BTN_REPEAT_PERIOD    15     /* 150 ms     */
#define BTN_QUEUE_SIZE       8

typedef struct {
    GPIO_TypeDef *port;
    uint16_t      pin;
    bool          repeat;           /* cho phép sự kiện lặp */
} btn_hw_t;

typedef struct {
    uint8_t  cnt;                   /* bộ đếm chống dội */
    bool     stable;                /* trạng thái ổn định: true = đang nhấn */
    uint16_t hold;                  /* thời gian giữ (tick) */
    bool     long_sent;
} btn_state_t;

static const btn_hw_t s_hw[BTN_COUNT] = {
    [BTN_UP]    = { BTN_UP_GPIO_Port,    BTN_UP_Pin,    true  },
    [BTN_DOWN]  = { BTN_DOWN_GPIO_Port,  BTN_DOWN_Pin,  true  },
    [BTN_ENTER] = { BTN_ENTER_GPIO_Port, BTN_ENTER_Pin, false },
    [BTN_EXIT]  = { BTN_EXIT_GPIO_Port,  BTN_EXIT_Pin,  false },
};

static btn_state_t s_btn[BTN_COUNT];
static btn_event_t s_queue[BTN_QUEUE_SIZE];
static volatile uint8_t s_head, s_tail;

static void push_event(btn_id_t id, btn_evt_type_t type)
{
    uint8_t next = (uint8_t)((s_head + 1) % BTN_QUEUE_SIZE);
    if (next == s_tail) return;             /* đầy – bỏ sự kiện */
    s_queue[s_head].id = id;
    s_queue[s_head].type = type;
    s_head = next;
}

void Button_Init(void)
{
    for (int i = 0; i < BTN_COUNT; i++) {
        s_btn[i] = (btn_state_t){0};
    }
    s_head = s_tail = 0;
}

void Button_Scan(void)
{
    for (int i = 0; i < BTN_COUNT; i++) {
        btn_state_t *b = &s_btn[i];
        bool raw = (HAL_GPIO_ReadPin(s_hw[i].port, s_hw[i].pin) == GPIO_PIN_RESET);

        /* Chống dội: trạng thái mới phải giữ đủ BTN_DEBOUNCE_TICKS */
        if (raw != b->stable) {
            if (++b->cnt >= BTN_DEBOUNCE_TICKS) {
                b->cnt = 0;
                b->stable = raw;
                b->hold = 0;
                b->long_sent = false;
                if (raw) push_event((btn_id_t)i, BTN_EVT_CLICK);
            }
        } else {
            b->cnt = 0;
        }

        /* Xử lý giữ nút */
        if (b->stable) {
            if (b->hold < 0xFFFF) b->hold++;
            if (s_hw[i].repeat) {
                if (b->hold >= BTN_REPEAT_START &&
                    ((b->hold - BTN_REPEAT_START) % BTN_REPEAT_PERIOD) == 0) {
                    push_event((btn_id_t)i, BTN_EVT_REPEAT);
                }
            } else if (!b->long_sent && b->hold >= BTN_LONG_TICKS) {
                b->long_sent = true;
                push_event((btn_id_t)i, BTN_EVT_LONG);
            }
        }
    }
}

bool Button_GetEvent(btn_event_t *evt)
{
    if (s_tail == s_head) return false;
    *evt = s_queue[s_tail];
    s_tail = (uint8_t)((s_tail + 1) % BTN_QUEUE_SIZE);
    return true;
}

bool Button_IsPressed(btn_id_t id)
{
    return (id < BTN_COUNT) ? s_btn[id].stable : false;
}
