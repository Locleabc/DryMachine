/**
 * @file    app.c
 * @brief   Bộ lập lịch cooperative theo HAL_GetTick() – không dùng RTOS.
 */
#include "app.h"
#include "app_config.h"
#include "bsp_relay.h"
#include "bsp_button.h"
#include "drv_ili9341.h"
#include "settings.h"
#include "ctrl_dryer.h"
#include "ui_menu.h"
#include "util_fmt.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    uint32_t period;
    uint32_t last;
} task_t;

enum { T_BTN = 0, T_SENSOR, T_HUM, T_CTRL, T_UI, T_LOG, T_LED, T_COUNT };

static task_t s_task[T_COUNT] = {
    [T_BTN]    = { TASK_BUTTON_MS, 0 },
    [T_SENSOR] = { TASK_SENSOR_MS, 0 },
    [T_HUM]    = { TASK_HUM_MS,    0 },
    [T_CTRL]   = { TASK_CTRL_MS,   0 },
    [T_UI]     = { TASK_UI_MS,     0 },
    [T_LOG]    = { TASK_LOG_MS,    0 },
    [T_LED]    = { TASK_LED_MS,    0 },
};

static app_meas_t   s_meas;
static ctrl_status_t s_status;

static bool task_due(int id, uint32_t now)
{
    if (now - s_task[id].last >= s_task[id].period) {
        s_task[id].last = now;
        return true;
    }
    return false;
}

void App_Log(const char *fmt, ...)
{
    char buf[128];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf) - 2, fmt, ap);
    va_end(ap);
    if (n < 0) return;
    if (n > (int)sizeof(buf) - 3) n = sizeof(buf) - 3;
    buf[n++] = '\r';
    buf[n++] = '\n';
    HAL_UART_Transmit(DEBUG_UART, (uint8_t *)buf, (uint16_t)n, 50);
}

static void log_status(void)
{
    char t[12], h[12], p[12];
    App_Log("T=%s H=%s P=%s ST=%d C=%d QN=%d QL=%d F=0x%02X W=0x%02X",
            s_meas.temp.ok  ? Fmt_Float(t, sizeof(t), s_meas.temp.temp_c, 1) : "ERR",
            s_meas.hum.ok   ? Fmt_Float(h, sizeof(h), s_meas.hum.rh, 1)       : "ERR",
            s_meas.press.ok ? Fmt_Float(p, sizeof(p), s_meas.press.bar, 2)    : "ERR",
            s_status.state, s_status.comp, s_status.fan_cond, s_status.fan_evap,
            s_status.faults, s_status.warnings);
}

void App_Init(void)
{
    Relay_Init();                 /* đảm bảo tất cả relay OFF trước tiên */
    Button_Init();
    Settings_Init();

    ILI9341_Init();
    ILI9341_FillScreen(C_BLACK);
    ILI9341_DrawString(40, 100, "DRY MACHINE", C_WHITE, C_BLACK, 3);
    ILI9341_DrawString(80, 140, "Dang khoi dong...", C_GRAY, C_BLACK, 1);

    MAX31865_Init();
    Pressure_Init();
    Humidity_Init();

    memset(&s_meas, 0, sizeof(s_meas));
    uint32_t now = HAL_GetTick();
    Ctrl_Init(now);
    UI_Init();
    for (int i = 0; i < T_COUNT; i++) s_task[i].last = now;

    App_Log("DryMachine boot, settings v%u", (unsigned)g_settings.version);
}

void App_Loop(void)
{
    uint32_t now = HAL_GetTick();

    if (task_due(T_BTN, now)) Button_Scan();

    Humidity_Poll();                           /* xử lý phản hồi RS485 */

    if (task_due(T_SENSOR, now)) {
        MAX31865_Read(&s_meas.temp);
        Pressure_Read(&s_meas.press);
    }

    if (task_due(T_HUM, now)) {
        Humidity_Get(&s_meas.hum);             /* kết quả lần trước */
        Humidity_Request();                    /* gửi yêu cầu mới */
    }

    if (task_due(T_CTRL, now)) {
        ctrl_input_t in = {
            .temp_ok  = s_meas.temp.ok,  .temp  = s_meas.temp.temp_c,
            .hum_ok   = s_meas.hum.ok,   .hum   = s_meas.hum.rh,
            .press_ok = s_meas.press.ok, .press = s_meas.press.bar,
        };
        Ctrl_Update(&in, now);
        Ctrl_GetStatus(&s_status);
    }

    if (task_due(T_UI, now)) {
        btn_event_t e;
        while (Button_GetEvent(&e)) UI_HandleButton(&e);
        UI_Update(&s_meas, &s_status);
    }

    if (task_due(T_LOG, now)) log_status();

    if (task_due(T_LED, now)) HAL_GPIO_TogglePin(LED_RUN_GPIO_Port, LED_RUN_Pin);
}
