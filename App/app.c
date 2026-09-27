/**
 * @file    app.c
 * @brief   Keo nối (glue): khởi tạo module, chuyển dữ liệu giữa các tầng.
 *          KHÔNG chứa logic điều khiển hay vẽ màn hình – chỉ nối dây.
 *
 *   board ──cấu hình──> drivers / sensors / settings
 *   sensors ──sensors_data_t──> dryer_input_t ──DryerCtrl_Step──> dryer_output_t ──> relay
 *   sensors + ctrl + settings ──> ui_view_t ──> UI ; UI ──ui_cmd_t──> ctrl / settings
 */
#include "app.h"
#include "board.h"
#include "relay.h"
#include "button.h"
#include "ili9341.h"
#include "sensors.h"
#include "settings.h"
#include "dryer_ctrl.h"
#include "ui.h"
#include "log.h"
#include "sched.h"
#include "util_fmt.h"

static dryer_ctrl_t   s_ctrl;
static dryer_status_t s_status;

/* ================= Adapter: settings → UI ================= */
static uint8_t par_count(void) { return Settings_ParamCount(); }

static bool par_desc(uint8_t idx, ui_param_desc_t *o)
{
    const settings_param_t *p = Settings_Param(idx);
    if (!p) return false;
    o->name = p->name; o->unit = p->unit;
    o->min = p->min;   o->max = p->max; o->step = p->step; o->dec = p->dec;
    return true;
}

static const ui_param_if_t s_ui_params = {
    .count = par_count, .desc = par_desc,
    .get = Settings_GetValue, .set = Settings_SetValue,
};

/* ================= Lệnh từ UI ================= */
static bool on_ui_cmd(ui_cmd_t cmd)
{
    switch (cmd) {
    case UI_CMD_START_STOP:
        if (s_status.state == DRYER_FAULT) { UI_Message("DANG LOI: GIU EXIT"); return false; }
        DryerCtrl_Command(&s_ctrl, DRYER_CMD_TOGGLE);
        UI_Message(s_status.state == DRYER_IDLE ? "BAT DAU SAY" : "DUNG SAY");
        Log_Printf("UI: start/stop");
        return true;
    case UI_CMD_RESET_FAULT:
        DryerCtrl_Command(&s_ctrl, DRYER_CMD_RESET_FAULT);
        UI_Message("DA RESET LOI");
        return true;
    case UI_CMD_SAVE_SETTINGS: {
        bool ok = Settings_Save();
        Log_Printf("Settings save: %s", ok ? "OK" : "FAIL");
        return ok;
    }
    }
    return false;
}

/* ================= Tasks ================= */
static void task_buttons(uint32_t now)
{
    (void)now;
    Button_Scan();
}

static void task_ctrl(uint32_t now)
{
    const settings_t *set = Settings_Get();
    const sensors_data_t *d = Sensors_Data();

    sensors_calib_t cal = { set->temp_offset, set->hum_offset };  /* áp dụng bù ngay khi sửa */
    Sensors_SetCalib(&cal);

    dryer_input_t in = {
        .temp_ok  = d->temp.ok,  .temp  = d->temp.value,
        .hum_ok   = d->hum.ok,   .hum   = d->hum.value,
        .press_ok = d->press.ok, .press = d->press.value,
    };
    dryer_output_t out;
    DryerCtrl_Step(&s_ctrl, &set->ctrl, &in, now, &out);
    DryerCtrl_GetStatus(&s_ctrl, &s_status);

    Relay_Set(RLY_ID_COMP,     out.comp);
    Relay_Set(RLY_ID_FAN_COND, out.fan_cond);
    Relay_Set(RLY_ID_FAN_EVAP, out.fan_evap);
}

static void task_ui(uint32_t now)
{
    static const ui_key_t   keymap[BTN_ID_COUNT] = {
        [BTN_ID_UP] = UI_KEY_UP, [BTN_ID_DOWN] = UI_KEY_DOWN,
        [BTN_ID_ENTER] = UI_KEY_ENTER, [BTN_ID_EXIT] = UI_KEY_EXIT,
    };
    static const ui_press_t pressmap[] = {
        [BUTTON_EVT_CLICK] = UI_PRESS_SHORT, [BUTTON_EVT_LONG] = UI_PRESS_LONG,
        [BUTTON_EVT_REPEAT] = UI_PRESS_REPEAT,
    };
    button_evt_t e;
    while (Button_GetEvent(&e)) {
        if (e.id < BTN_ID_COUNT) UI_Key(keymap[e.id], pressmap[e.type]);
    }

    const sensors_data_t *d = Sensors_Data();
    const dryer_params_t *p = &Settings_Get()->ctrl;
    ui_view_t v = {
        .temp_ok  = d->temp.ok,  .temp  = d->temp.value,  .temp_set = p->temp_set,
        .hum_ok   = d->hum.ok,   .hum   = d->hum.value,   .hum_set  = p->hum_set,
        .hum_reached = s_status.target_reached,
        .press_ok = d->press.ok, .press = d->press.value,
        .comp = s_status.out.comp, .fan_cond = s_status.out.fan_cond, .fan_evap = s_status.out.fan_evap,
        .state_text = DryerCtrl_StateName(s_status.state),
        .running = (s_status.state == DRYER_STARTING || s_status.state == DRYER_RUNNING),
        .run_s = s_status.run_s,
        .comp_wait_s = s_status.comp_demand ? s_status.comp_wait_s : 0,
    };
    if (s_status.faults) {
        v.alarm = UI_ALARM_FAULT; v.alarm_text = DryerCtrl_FaultText(s_status.faults);
    } else if (s_status.warnings & DRYER_WARN_HUM_SENSOR) {
        v.alarm = UI_ALARM_WARN;  v.alarm_text = "CANH BAO: MAT SHT45";
    }
    UI_Update(&v, now);
}

static void task_log(uint32_t now)
{
    (void)now;
    const sensors_data_t *d = Sensors_Data();
    char t[12], h[12], p[12];
    Log_Printf("T=%s H=%s P=%s ST=%s MN=%d QN=%d QL=%d F=0x%02X W=0x%02X SHTerr=%lu",
               d->temp.ok  ? Fmt_Float(t, sizeof(t), d->temp.value, 1)  : "ERR",
               d->hum.ok   ? Fmt_Float(h, sizeof(h), d->hum.value, 1)   : "ERR",
               d->press.ok ? Fmt_Float(p, sizeof(p), d->press.value, 2) : "ERR",
               DryerCtrl_StateName(s_status.state),
               s_status.out.comp, s_status.out.fan_cond, s_status.out.fan_evap,
               s_status.faults, s_status.warnings, (unsigned long)d->sht_errors);
}

static void task_led(uint32_t now)
{
    (void)now;
    Board_LedToggle();
}

static sched_task_t s_tasks[] = {
    { BUTTON_SCAN_MS, task_buttons, 0 },
    { 200,            task_ctrl,    0 },
    { 100,            task_ui,      0 },
    { 2000,           task_log,     0 },
    { 500,            task_led,     0 },
};
#define TASK_COUNT  ((uint8_t)(sizeof(s_tasks) / sizeof(s_tasks[0])))

/* ================= Entry ================= */
void App_Init(void)
{
    Relay_Init(board_relays, RLY_ID_COUNT);          /* relay OFF trước tiên */
    Button_Init(board_buttons, BTN_ID_COUNT);
    Log_Init(Board_LogWrite);
    Settings_Init(&board_settings_flash);

    ILI9341_Init(&board_lcd);
    ILI9341_FillScreen(C_BLACK);
    ILI9341_DrawString(40, 100, "DRY MACHINE", C_WHITE, C_BLACK, 3);
    ILI9341_DrawString(100, 140, "Dang khoi dong...", C_GRAY, C_BLACK, 1);

    static const sensors_cfg_t sensors_cfg = {
        .pt100 = &board_pt100, .sht = &board_sht45, .press = &board_press,
        .bus_recover = Board_I2cRecover,
    };
    Sensors_Init(&sensors_cfg);

    uint32_t now = Board_Millis();
    DryerCtrl_Init(&s_ctrl, now);
    DryerCtrl_GetStatus(&s_ctrl, &s_status);
    UI_Init(&s_ui_params, on_ui_cmd);
    Sched_Init(s_tasks, TASK_COUNT, now);

    Log_Printf("DryMachine boot, settings v%u, SHT45 serial %08lX",
               (unsigned)Settings_Get()->version, (unsigned long)Sensors_Data()->sht_serial);
}

void App_Loop(void)
{
    uint32_t now = Board_Millis();
    Sensors_Process(now);
    Sched_Run(s_tasks, TASK_COUNT, now);
}
