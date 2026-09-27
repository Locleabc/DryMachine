/**
 * @file    app.c
 * @brief   Keo nối (glue): khởi tạo module, chuyển dữ liệu giữa các tầng.
 *          KHÔNG chứa logic điều khiển hay vẽ màn hình – chỉ nối dây.
 *
 *   board ──cấu hình──> drivers / sensors / settings / fault_log
 *   sensors ──> dryer_input_t ──DryerCtrl_Step──> dryer_output_t ──> relay
 *   sensors + ctrl + settings + RTC + fault_log ──> ui_view_t ──> UI
 *   UI ──ui_cmd_t──> ctrl / settings / RTC / fault_log
 */
#include "app.h"
#include "board.h"
#include "relay.h"
#include "button.h"
#include "ili9341.h"
#include "sensors.h"
#include "settings.h"
#include "presets.h"
#include "fault_log.h"
#include "datetime.h"
#include "dryer_ctrl.h"
#include "ui.h"
#include "log.h"
#include "sched.h"
#include "util_fmt.h"
#include <stdio.h>

static dryer_ctrl_t   s_ctrl;
static dryer_status_t s_status;
static uint16_t       s_prev_faults;

/* ================= Adapter: settings → menu kỹ thuật ================= */
static uint8_t par_count(void) { return Settings_ParamCount(); }

static bool par_desc(uint8_t idx, ui_param_desc_t *o)
{
    const settings_param_t *p = Settings_Param(idx);
    if (!p) return false;
    o->name = p->name; o->unit = p->unit;
    o->min = p->min;   o->max = p->max; o->step = p->step; o->dec = p->dec;
    return true;
}

static const ui_param_if_t s_tech_params = {
    .count = par_count, .desc = par_desc,
    .get = Settings_GetValue, .set = Settings_SetValue,
};

static const char *s_preset_names[PRESET_COUNT];

/* ================= Thời gian ================= */
static bool clock_now(uint32_t *epoch)
{
    return Board_RtcRead(epoch);
}

/* ================= Lệnh từ UI ================= */
static bool on_ui_cmd(const ui_cmd_t *cmd)
{
    switch (cmd->type) {
    case UI_CMD_START_STOP:
        if (s_status.state == DRYER_FAULT) { UI_Message("DANG LOI - XEM TRANG 4"); return false; }
        if (s_status.state == DRYER_STOPPING) return false;
        DryerCtrl_Command(&s_ctrl, DRYER_CMD_TOGGLE);
        UI_Message(s_status.state == DRYER_IDLE ? "BAT DAU SAY" : "DUNG SAY");
        Log_Printf("UI: start/stop");
        return true;

    case UI_CMD_RESET_FAULT:
        DryerCtrl_Command(&s_ctrl, DRYER_CMD_RESET_FAULT);
        UI_Message("DA XOA LOI");
        return true;

    case UI_CMD_CLEAR_HISTORY:
        return FaultLog_Clear();

    case UI_CMD_SELECT_PRESET:
        Settings_SelectPreset(cmd->u.preset.idx);
        Log_Printf("Preset -> %u", (unsigned)cmd->u.preset.idx);
        return Settings_Save();

    case UI_CMD_SET_PRESET: {
        bool in_range = Settings_SetPresetValues(cmd->u.preset.idx, cmd->u.preset.temp, cmd->u.preset.hum);
        if (cmd->u.preset.select) Settings_SelectPreset(cmd->u.preset.idx);
        bool saved = Settings_Save();
        return in_range && saved;
    }

    case UI_CMD_SET_DRY_TIME:
        Settings_SetDryTimeMin(cmd->u.minutes);
        return Settings_Save();

    case UI_CMD_SET_CLOCK: {
        datetime_t dt = { cmd->u.clock.year, cmd->u.clock.mon, cmd->u.clock.day,
                          cmd->u.clock.hour, cmd->u.clock.min, 0 };
        if (!DateTime_IsValid(&dt)) return false;
        Board_RtcWrite(DateTime_ToEpoch(&dt));
        return true;
    }

    case UI_CMD_SAVE_SETTINGS: {
        bool ok = Settings_Save();
        Log_Printf("Settings save: %s", ok ? "OK" : "FAIL");
        return ok;
    }
    }
    return false;
}

/* ================= Lịch sử lỗi: ghi mỗi bit lỗi mới xuất hiện ================= */
static void log_new_faults(void)
{
    uint16_t added = (uint16_t)(s_status.faults & ~s_prev_faults);
    s_prev_faults = s_status.faults;
    if (!added) return;

    uint32_t t = 0;
    if (!clock_now(&t)) t = 0;
    for (uint16_t bit = 1; bit; bit <<= 1) {
        if (added & bit) {
            FaultLog_Add(t, bit);
            Log_Printf("FAULT: %s", DryerCtrl_FaultText(bit));
        }
    }
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

    sensors_calib_t cal = { set->temp_offset, set->hum_offset };
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

    log_new_faults();
}

static ui_state_t map_state(dryer_state_t s)
{
    switch (s) {
    case DRYER_STARTING: return UI_ST_STARTING;
    case DRYER_RUNNING:  return UI_ST_RUNNING;
    case DRYER_STOPPING: return UI_ST_STOPPING;
    case DRYER_FAULT:    return UI_ST_FAULT;
    default:             return UI_ST_IDLE;
    }
}

static void build_view(ui_view_t *v)
{
    const sensors_data_t *d = Sensors_Data();
    const settings_t *set = Settings_Get();

    *v = (ui_view_t){0};
    v->temp_ok  = d->temp.ok;  v->temp  = d->temp.value;
    v->hum_ok   = d->hum.ok;   v->hum   = d->hum.value;
    v->press_ok = d->press.ok; v->press = d->press.value;

    v->temp_set = set->ctrl.temp_set;
    v->hum_set  = set->ctrl.hum_set;
    v->preset   = set->preset;
    for (uint8_t i = 0; i < PRESET_COUNT && i < UI_PRESET_MAX; i++) {
        v->preset_temp[i] = set->preset_temp[i];
        v->preset_hum[i]  = set->preset_hum[i];
    }

    v->state        = map_state(s_status.state);
    v->comp         = s_status.out.comp;
    v->fan_cond     = s_status.out.fan_cond;
    v->fan_evap     = s_status.out.fan_evap;
    v->run_s        = s_status.run_s;
    v->dry_time_min = Settings_DryTimeMin();
    v->comp_wait_s  = s_status.comp_demand ? s_status.comp_wait_s : 0;
    v->fault_text   = s_status.faults ? DryerCtrl_FaultText(s_status.faults) : NULL;
    v->warn_text    = (s_status.warnings & DRYER_WARN_HUM_SENSOR) ? "CANH BAO: MAT SHT45" : NULL;

    uint32_t epoch;
    v->clock_ok = clock_now(&epoch);
    if (v->clock_ok) {
        datetime_t dt;
        DateTime_FromEpoch(epoch, &dt);
        v->year = dt.year; v->mon = dt.mon; v->day = dt.day;
        v->hour = dt.hour; v->min = dt.min; v->sec = dt.sec;
    }

    uint8_t n = FaultLog_Count();
    v->hist_count = (n > UI_HIST_ROWS) ? UI_HIST_ROWS : n;
    for (uint8_t i = 0; i < v->hist_count; i++) {
        fault_log_entry_t e;
        if (!FaultLog_Get(i, &e)) break;
        if (e.time) {
            datetime_t dt;
            DateTime_FromEpoch(e.time, &dt);
            snprintf(v->hist[i].when, sizeof(v->hist[i].when), "%02u/%02u %02u:%02u",
                     (unsigned)(dt.day % 100u), (unsigned)(dt.mon % 100u), (unsigned)(dt.hour % 100u), (unsigned)(dt.min % 100u));
        } else {
            snprintf(v->hist[i].when, sizeof(v->hist[i].when), "--/-- --:--");
        }
        v->hist[i].text = DryerCtrl_FaultText(e.code);
    }
}

static void task_ui(uint32_t now)
{
    static const ui_key_t keymap[BTN_ID_COUNT] = {
        [BTN_ID_UP] = UI_KEY_UP, [BTN_ID_DOWN] = UI_KEY_DOWN,
        [BTN_ID_ENTER] = UI_KEY_ENTER, [BTN_ID_EXIT] = UI_KEY_EXIT,
    };
    static const ui_press_t pressmap[] = {
        [BUTTON_EVT_CLICK] = UI_PRESS_SHORT, [BUTTON_EVT_LONG] = UI_PRESS_LONG,
        [BUTTON_EVT_REPEAT] = UI_PRESS_REPEAT,
    };
    static ui_view_t v;

    build_view(&v);
    button_evt_t e;
    while (Button_GetEvent(&e)) {
        if (e.id < BTN_ID_COUNT) UI_Key(keymap[e.id], pressmap[e.type]);
    }
    build_view(&v);                   /* lệnh vừa xử lý có thể đổi dữ liệu */
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
    Board_RtcInit();
    Settings_Init(&board_settings_flash);
    FaultLog_Init(&board_faultlog_flash);

    ILI9341_Init(&board_lcd);
    ILI9341_FillScreen(C_BLACK);
    ILI9341_DrawString(52, 100, "DRY MACHINE", C_WHITE, C_BLACK, 3);
    ILI9341_DrawString(109, 140, "Dang khoi dong...", C_GRAY, C_BLACK, 1);

    static const sensors_cfg_t sensors_cfg = {
        .pt100 = &board_pt100, .sht = &board_sht45, .press = &board_press,
        .bus_recover = Board_I2cRecover,
    };
    Sensors_Init(&sensors_cfg);

    uint32_t now = Board_Millis();
    DryerCtrl_Init(&s_ctrl, now);
    DryerCtrl_GetStatus(&s_ctrl, &s_status);

    for (uint8_t i = 0; i < PRESET_COUNT; i++) s_preset_names[i] = g_preset_defs[i].name;
    static const ui_config_t ui_cfg = {
        .preset_names = s_preset_names, .preset_count = PRESET_COUNT,
        .tech = &s_tech_params, .on_cmd = on_ui_cmd,
    };
    UI_Init(&ui_cfg);
    Sched_Init(s_tasks, TASK_COUNT, now);

    Log_Printf("DryMachine boot, settings v%u, SHT45 serial %08lX, log %u",
               (unsigned)Settings_Get()->version, (unsigned long)Sensors_Data()->sht_serial,
               (unsigned)FaultLog_Count());
}

void App_Loop(void)
{
    uint32_t now = Board_Millis();
    Sensors_Process(now);
    Sched_Run(s_tasks, TASK_COUNT, now);
}
