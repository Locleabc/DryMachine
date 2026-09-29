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
#include "ili9341_text.h"
#include "sensors.h"
#include "settings.h"
#include "presets.h"
#include "fault_log.h"
#include "datetime.h"
#include "dryer_ctrl.h"
#include "fan_speed.h"
#include "sim_plant.h"
#include "ui.h"
#include "log.h"
#include "sched.h"
#include "util_fmt.h"
#include <stdio.h>
#include <string.h>

static dryer_ctrl_t   s_ctrl;
static dryer_status_t s_status;
static uint16_t       s_prev_faults;
static fan_speed_t    s_fan;             /* khoá liên động 5 relay tốc độ quạt dàn nóng */
static uint8_t        s_fan_actual;      /* cấp quạt đang thực sự đóng relay */
static bool           s_prev_finished;
static bool           s_prev_cooling;    /* đang làm mát do quá nhiệt (bước trước) */

/* ================= Chạy giả lập (không cần cảm biến) =================
 * Mở: giữ EXIT 3 s ở trang 2 (Chạy/Dừng). Chỉ lưu RAM – khởi động lại luôn TẮT giả lập.
 *  - Mô hình : nhiệt độ/độ ẩm/áp suất do sim_plant tính theo máy nén + quạt
 *  - Chỉnh tay: người dùng đặt nhiệt độ/độ ẩm trong danh sách GIẢ LẬP
 *  - Tua nhanh: đồng hồ của bộ điều khiển chạy x1/x10/x60/x300 (quạt 5 cấp vẫn theo thời gian thật)
 *  - Tạo lỗi : mất cảm biến nhiệt / ẩm, áp suất cao – để thử bảo vệ
 *  - Relay thật: mặc định BẬT – chân ra + relay đóng theo bộ điều khiển để đo điện áp thực tế;
 *                 chuyển Tắt (danh sách GIẢ LẬP hoặc ENTER ở trang 6) để chỉ hiện trên màn hình */
enum { SIM_OFF = 0, SIM_MODEL, SIM_MANUAL };
enum { SIMF_NONE = 0, SIMF_TEMP, SIMF_HUM, SIMF_PRESS };
static const char *const s_sim_mode_ch[]  = { "Tắt", "Mô hình", "Chỉnh tay" };
static const char *const s_sim_speed_ch[] = { "x1", "x10", "x60", "x300" };
static const uint16_t    s_sim_speed[]    = { 1, 10, 60, 300 };
static const char *const s_sim_fault_ch[] = { "Không", "Mất CB nhiệt", "Mất CB ẩm", "Áp suất cao" };
static const char *const s_onoff_ch[]     = { "Tắt", "Bật" };

static struct {
    uint8_t     mode, speed_idx, fault, relay;
    sim_plant_t plant;
    float       man_temp, man_hum;
    char        text[40];
} s_sim = { .relay = 1 };

static uint32_t s_vclock, s_last_tick;     /* đồng hồ của bộ điều khiển (có thể tua nhanh) */

typedef struct { bool t_ok, h_ok, p_ok; float t, h, p; } meas_t;

static void get_meas(meas_t *m)
{
    if (s_sim.mode == SIM_OFF) {
        const sensors_data_t *d = Sensors_Data();
        m->t_ok = d->temp.ok;  m->t = d->temp.value;
        m->h_ok = d->hum.ok;   m->h = d->hum.value;
        m->p_ok = d->press.ok; m->p = d->press.value;
        return;
    }
    m->t_ok = m->h_ok = m->p_ok = true;
    m->p = s_sim.plant.press;
    if (s_sim.mode == SIM_MODEL) { m->t = s_sim.plant.temp; m->h = s_sim.plant.hum; }
    else                         { m->t = s_sim.man_temp;   m->h = s_sim.man_hum;   }
    switch (s_sim.fault) {
    case SIMF_TEMP:  m->t_ok = false; break;
    case SIMF_HUM:   m->h_ok = false; break;
    case SIMF_PRESS: m->p = Settings_Get()->ctrl.p_high + 2.0f; break;
    default: break;
    }
}

static const ui_param_desc_t s_sim_desc[] = {
    { "Giả lập",     "",    0, 2,   1,    0, s_sim_mode_ch  },
    { "Tua nhanh",   "",    0, 3,   1,    0, s_sim_speed_ch },
    { "Nhiệt độ",    "°C",  0, 100, 0.5f, 1, NULL },
    { "Độ ẩm",       "%",   0, 100, 1,    0, NULL },
    { "Tạo lỗi",     "",    0, 3,   1,    0, s_sim_fault_ch },
    { "Relay thật",  "",    0, 1,   1,    0, s_onoff_ch },
};
#define SIM_ITEMS  ((uint8_t)(sizeof(s_sim_desc) / sizeof(s_sim_desc[0])))

static uint8_t sim_count(void) { return SIM_ITEMS; }
static bool sim_desc(uint8_t i, ui_param_desc_t *o) { if (i >= SIM_ITEMS) return false; *o = s_sim_desc[i]; return true; }
static float sim_get(uint8_t i)
{
    switch (i) {
    case 0: return s_sim.mode;
    case 1: return s_sim.speed_idx;
    case 2: return (s_sim.mode == SIM_MANUAL) ? s_sim.man_temp : s_sim.plant.temp;
    case 3: return (s_sim.mode == SIM_MANUAL) ? s_sim.man_hum  : s_sim.plant.hum;
    case 4: return s_sim.fault;
    case 5: return s_sim.relay;
    default: return 0;
    }
}
static void sim_set(uint8_t i, float v)
{
    uint8_t u = (uint8_t)(v + 0.5f);
    switch (i) {
    case 0:
        if (s_sim.mode == SIM_OFF && u != SIM_OFF) {          /* bật giả lập: khởi tạo buồng 30 °C / 65 % */
            SimPlant_Init(&s_sim.plant, 30.0f, 65.0f);
            s_sim.man_temp = 30.0f; s_sim.man_hum = 65.0f;
            /* chưa gắn cảm biến → lúc khởi động đã báo "Mất cảm biến nhiệt" và khoá máy;
             * vào giả lập thì xoá lỗi đó để chạy được (giá trị giả lập luôn hợp lệ) */
            DryerCtrl_Command(&s_ctrl, DRYER_CMD_RESET_FAULT);
        }
        if (u == SIM_OFF) { s_sim.speed_idx = 0; s_sim.fault = SIMF_NONE; }
        s_sim.mode = u;
        Log_Printf("SIM mode=%u", (unsigned)u);
        break;
    case 1: s_sim.speed_idx = u; break;
    case 2: s_sim.man_temp = v; s_sim.plant.temp = v; break;
    case 3: s_sim.man_hum  = v; s_sim.plant.hum  = v; break;
    case 4: s_sim.fault = u; break;
    case 5: s_sim.relay = u; break;
    default: break;
    }
}
static const ui_param_if_t s_sim_params = { sim_count, sim_desc, sim_get, sim_set };

/* ================= Adapter: settings → danh sách thông số trên UI ================= */
/* Mỗi nhóm thông số (SETTINGS_GROUP_*) là một ui_param_if_t; chỉ số trong nhóm → chỉ số toàn cục */
#define GROUP_ADAPTER(NAME, GROUP)                                                        \
    static uint8_t NAME##_count(void) { return Settings_GroupCount(GROUP); }               \
    static bool NAME##_desc(uint8_t i, ui_param_desc_t *o) {                               \
        int g = Settings_GroupIndex(GROUP, i);                                              \
        const settings_param_t *p = (g < 0) ? 0 : Settings_Param((uint8_t)g);              \
        if (!p) return false;                                                              \
        o->name = p->name; o->unit = p->unit; o->min = p->min; o->max = p->max;            \
        o->step = p->step; o->dec = p->dec; o->choices = p->choices;                       \
        return true; }                                                                     \
    static float NAME##_get(uint8_t i) {                                                   \
        int g = Settings_GroupIndex(GROUP, i); return (g < 0) ? 0.0f : Settings_GetValue((uint8_t)g); } \
    static void NAME##_set(uint8_t i, float v) {                                           \
        int g = Settings_GroupIndex(GROUP, i); if (g >= 0) Settings_SetValue((uint8_t)g, v); } \
    static const ui_param_if_t NAME = { NAME##_count, NAME##_desc, NAME##_get, NAME##_set };

GROUP_ADAPTER(s_tech_params, SETTINGS_GROUP_TECH)
GROUP_ADAPTER(s_proc_params, SETTINGS_GROUP_PROCESS)

static const char *s_preset_names[PRESET_COUNT];

/* trang trạng thái đầu ra – thứ tự = RLY_ID_* */
static const char *const s_out_names[RLY_ID_COUNT] = {
    "Máy nén", "Quạt dàn lạnh",
    "Quạt nóng cấp 1", "Quạt nóng cấp 2", "Quạt nóng cấp 3", "Quạt nóng cấp 4", "Quạt nóng cấp 5",
};
static const char *const s_out_pins[RLY_ID_COUNT] = {
    "IN1 · PB5", "IN2 · PB7", "IN3 · PB6", "IN4 · PA1", "IN5 · PA2", "IN6 · PA3", "IN7 · PB4",
};

/* ================= Thời gian ================= */
static bool clock_now(uint32_t *epoch)
{
    return Board_RtcRead(epoch);
}

/* ================= Test đầu ra (trang 6) =================
 *  Bật/tắt tay từng relay khi máy KHÔNG chạy (Đang dừng hoặc Lỗi).
 *  - quạt dàn nóng vẫn qua khoá liên động: chỉ 1 cấp, đổi cấp nghỉ 1 s
 *  - máy nén vẫn giữ thời gian chờ bật lại (comp_min_off)
 *  - luôn đóng relay thật (kể cả khi giả lập), thoát test hoặc 10 phút không bấm → tắt hết */
#define OUT_TEST_TIMEOUT_MS  (10u * 60u * 1000u)
static struct {
    bool     on, comp, evap;
    uint8_t  fan;                 /* 0 = tắt, 1..5 */
    uint32_t last_key;
} s_test;
static uint32_t s_comp_off_tick;  /* lúc relay máy nén nhả gần nhất (thời gian thật) */
static uint8_t  s_out_cmd;        /* đầu ra đang được yêu cầu (bit = RLY_ID_*) */

static bool out_test_cmd(uint8_t op, uint8_t idx)
{
    uint32_t now = Board_Millis();
    switch (op) {
    case UI_TEST_BEGIN:
        if (DryerCtrl_OvertempCooling(&s_ctrl)) { UI_Message("Đang làm mát do quá nhiệt"); return false; }
        if (s_status.state != DRYER_IDLE && s_status.state != DRYER_FAULT) {
            UI_Message("Dừng máy trước khi test");
            return false;
        }
        memset(&s_test, 0, sizeof(s_test));
        s_test.on = true;
        s_test.last_key = now;
        Log_Printf("Test dau ra: bat dau");
        return true;
    case UI_TEST_TOGGLE:
        if (!s_test.on) return false;
        s_test.last_key = now;
        if (idx == RLY_ID_COMP) {
            if (!s_test.comp) {
                uint32_t wait_ms = (uint32_t)(Settings_Get()->ctrl.comp_min_off * 1000.0f);
                uint32_t off_ms  = now - s_comp_off_tick;
                if (!Relay_Get(RLY_ID_COMP) && off_ms < wait_ms) {
                    char m[40];
                    snprintf(m, sizeof(m), "Máy nén chờ %lu s", (unsigned long)((wait_ms - off_ms + 999u) / 1000u));
                    UI_Message(m);
                    return false;
                }
            }
            s_test.comp = !s_test.comp;
        } else if (idx == RLY_ID_FAN_EVAP) {
            s_test.evap = !s_test.evap;
        } else if (idx >= RLY_ID_FAN_S1 && idx < RLY_ID_FAN_S1 + DRYER_FAN_LEVELS) {
            uint8_t lv = (uint8_t)(idx - RLY_ID_FAN_S1 + 1);
            s_test.fan = (s_test.fan == lv) ? 0 : lv;          /* chỉ 1 cấp: bật cấp mới tự tắt cấp cũ */
        } else {
            return false;
        }
        Log_Printf("Test dau ra: %u -> comp=%u evap=%u fan=%u", (unsigned)idx,
                   (unsigned)s_test.comp, (unsigned)s_test.evap, (unsigned)s_test.fan);
        return true;
    case UI_TEST_END:
        if (s_test.on) Log_Printf("Test dau ra: ket thuc, tat het");
        memset(&s_test, 0, sizeof(s_test));
        return true;
    default:
        return false;
    }
}

/* ================= Lệnh từ UI ================= */
static bool on_ui_cmd(const ui_cmd_t *cmd)
{
    switch (cmd->type) {
    case UI_CMD_START_STOP:
        if (s_test.on) { UI_Message("Đang test đầu ra – thoát trước"); return false; }
        if (s_status.state == DRYER_FAULT) { UI_Message("Đang lỗi – xem trang 5"); return false; }
        if (s_status.state == DRYER_STOPPING) return false;
        DryerCtrl_Command(&s_ctrl, DRYER_CMD_TOGGLE);
        UI_Message(s_status.state == DRYER_IDLE ? "Bắt đầu sấy" : "Dừng sấy");
        Log_Printf("UI: start/stop");
        return true;

    case UI_CMD_RESET_FAULT:
        if (DryerCtrl_OvertempCooling(&s_ctrl)) {
            meas_t m;
            get_meas(&m);
            float rec = DryerCtrl_RecoverTemp(&Settings_Get()->ctrl);
            if (m.t_ok && m.t > rec) {
                char b[40], t[8];
                Fmt_Float(t, sizeof(t), rec, 0);
                snprintf(b, sizeof(b), "Chờ nguội về %s°C", t);
                UI_Message(b);
                return false;
            }
        }
        DryerCtrl_Command(&s_ctrl, DRYER_CMD_RESET_FAULT);
        UI_Message("Đã xoá lỗi");
        return true;

    case UI_CMD_OUT_TEST:
        return out_test_cmd(cmd->u.test.op, cmd->u.test.idx);

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

    sensors_calib_t cal = { set->temp_offset, set->hum_offset };
    Sensors_SetCalib(&cal);

    /* đồng hồ bộ điều khiển: ×1 bình thường, tua nhanh khi giả lập */
    uint32_t dt = now - s_last_tick;
    s_last_tick = now;
    uint16_t speed = (s_sim.mode != SIM_OFF) ? s_sim_speed[s_sim.speed_idx] : 1;
    s_vclock += dt * speed;

    meas_t m;
    get_meas(&m);
    dryer_input_t in = {
        .temp_ok  = m.t_ok, .temp  = m.t,
        .hum_ok   = m.h_ok, .hum   = m.h,
        .press_ok = m.p_ok, .press = m.p,
    };
    dryer_output_t out;
    DryerCtrl_Step(&s_ctrl, &set->ctrl, &in, s_vclock, &out);
    DryerCtrl_GetStatus(&s_ctrl, &s_status);

    /* test đầu ra: chỉ khi máy không chạy; hết giờ không bấm → thoát */
    bool cooling = DryerCtrl_OvertempCooling(&s_ctrl);
    if (s_prev_cooling && !cooling && s_status.state == DRYER_IDLE) {
        UI_Message("Đã nguội – hết quá nhiệt");
        Log_Printf("Qua nhiet: da nguoi, het loi");
    }
    s_prev_cooling = cooling;
    if (s_test.on && ((s_status.state != DRYER_IDLE && s_status.state != DRYER_FAULT) || cooling ||
                      now - s_test.last_key >= OUT_TEST_TIMEOUT_MS)) {
        out_test_cmd(UI_TEST_END, 0);
        UI_Message(cooling ? "Quá nhiệt – thoát test" : "Hết giờ test – đã tắt hết");
    }
    bool    want_comp = s_test.on ? s_test.comp : out.comp;
    bool    want_evap = s_test.on ? s_test.evap : out.fan_evap;
    uint8_t want_fan  = s_test.on ? s_test.fan  : out.fan_level;

    /* quạt dàn nóng 5 cấp: chỉ 1 relay đóng, đổi cấp có khoảng nghỉ (thời gian thật) */
    s_fan_actual = FanSpeed_Step(&s_fan, want_fan, now);

    if (s_sim.mode != SIM_OFF) {
        SimPlant_Step(&s_sim.plant, (float)(dt * speed) / 1000.0f, want_comp, s_fan_actual, want_evap);
        snprintf(s_sim.text, sizeof(s_sim.text), "GIẢ LẬP · %s %s%s", s_sim_mode_ch[s_sim.mode],
                 s_sim_speed_ch[s_sim.speed_idx], s_sim.relay ? " · relay BẬT" : "");
    }

    /* giả lập có thể không đóng relay; test đầu ra luôn đóng relay thật */
    bool drive = (s_sim.mode == SIM_OFF) || s_sim.relay || s_test.on;
    bool comp_was = Relay_Get(RLY_ID_COMP);
    Relay_Set(RLY_ID_COMP,     drive && want_comp);
    Relay_Set(RLY_ID_FAN_EVAP, drive && want_evap);
    for (uint8_t i = 0; i < DRYER_FAN_LEVELS; i++) {
        Relay_Set((uint8_t)(RLY_ID_FAN_S1 + i), drive && s_fan_actual == i + 1);
    }
    if (comp_was && !Relay_Get(RLY_ID_COMP)) s_comp_off_tick = now;

    s_out_cmd = (uint8_t)((want_comp ? 1u << RLY_ID_COMP : 0u) | (want_evap ? 1u << RLY_ID_FAN_EVAP : 0u));
    if (s_fan_actual >= 1 && s_fan_actual <= DRYER_FAN_LEVELS)
        s_out_cmd |= (uint8_t)(1u << (RLY_ID_FAN_S1 + s_fan_actual - 1));
    else if (want_fan >= 1 && want_fan <= DRYER_FAN_LEVELS)          /* đang nghỉ đổi cấp */
        s_out_cmd |= (uint8_t)(1u << (RLY_ID_FAN_S1 + want_fan - 1));

    log_new_faults();
    if (s_status.finished && !s_prev_finished) {
        UI_Message("Hoàn thành chu trình sấy");
        Log_Printf("Chu trinh hoan thanh");
    }
    s_prev_finished = s_status.finished;
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
    const settings_t *set = Settings_Get();
    meas_t m;
    get_meas(&m);

    *v = (ui_view_t){0};
    v->temp_ok  = m.t_ok; v->temp  = m.t;
    v->hum_ok   = m.h_ok; v->hum   = m.h;
    v->press_ok = m.p_ok; v->press = m.p;
    v->sim_text = (s_sim.mode != SIM_OFF) ? s_sim.text : NULL;

    v->temp_set = set->ctrl.temp_set;
    v->hum_set  = set->ctrl.hum_set;
    v->hum_reached = s_status.target_reached;
    v->preset   = set->preset;
    for (uint8_t i = 0; i < PRESET_COUNT && i < UI_PRESET_MAX; i++) {
        v->preset_temp[i] = set->preset_temp[i];
        v->preset_hum[i]  = set->preset_hum[i];
    }

    v->state        = map_state(s_status.state);
    v->comp         = s_status.out.comp;
    v->fan_level    = s_fan_actual;
    v->fan_evap     = s_status.out.fan_evap;
    v->out_cmd  = s_out_cmd;
    v->out_test = s_test.on;
    v->out_relay = 0;
    for (uint8_t i = 0; i < RLY_ID_COUNT; i++)
        if (Relay_Get(i)) v->out_relay |= (uint8_t)(1u << i);
    v->run_s        = s_status.run_s;
    v->phase_left_s = s_status.phase_left_s;
    v->phase_text   = DryerCtrl_PhaseName(s_status.phase);
    switch (s_status.phase) {
    case PH_GD1: case PH_AUTO_DRY:  v->stage = 0; break;
    case PH_GD2: case PH_AUTO_HOLD: v->stage = 1; break;
    case PH_GD3: case PH_AUTO_COOL: v->stage = 2; break;
    case PH_GD4: v->stage = 3; break;
    case PH_GD5: v->stage = 4; break;
    default:     v->stage = (s_status.state == DRYER_STARTING) ? 0 : -1; break;
    }
    bool active = (s_status.state == DRYER_STARTING || s_status.state == DRYER_RUNNING);
    v->manual         = active ? s_status.manual : (set->ctrl.mode > 0.5f);
    v->auto_fan       = (uint8_t)(set->ctrl.auto_fan + 0.5f);
    for (uint8_t i = 0; i < 5; i++) v->stage_fan[i] = (uint8_t)(set->ctrl.stage_fan[i] + 0.5f);
    v->gd3_min        = (uint16_t)(set->ctrl.gd3_min + 0.5f);
    v->gd4_min        = (uint16_t)(set->ctrl.gd4_min + 0.5f);
    v->end_temp       = set->ctrl.end_temp;
    v->temp_max       = set->ctrl.temp_max;
    v->comp_restart_s = (uint16_t)(set->ctrl.comp_min_off + 0.5f);
    v->dry_time_min = Settings_DryTimeMin();
    v->comp_wait_s  = s_status.comp_demand ? s_status.comp_wait_s : 0;
    v->fault_text   = s_status.faults ? DryerCtrl_FaultText(s_status.faults) : NULL;
    if (DryerCtrl_OvertempCooling(&s_ctrl)) {
        static char ot[40];
        char t[8];
        Fmt_Float(t, sizeof(t), DryerCtrl_RecoverTemp(&set->ctrl), 0);
        snprintf(ot, sizeof(ot), "Quá nhiệt · làm mát tới %s°C", t);
        v->fault_text = ot;
    }
    v->warn_text    = (s_status.warnings & DRYER_WARN_HUM_SENSOR) ? "Cảnh báo: mất cảm biến ẩm SHT45" : NULL;

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
    meas_t m;
    get_meas(&m);
    char t[12], h[12], p[12];
    Log_Printf("%sT=%s H=%s P=%s ST=%s PH=%d MN=%d QN=%d QL=%d F=0x%02X W=0x%02X SHTerr=%lu",
               s_sim.mode ? "[SIM] " : "",
               m.t_ok ? Fmt_Float(t, sizeof(t), m.t, 1) : "ERR",
               m.h_ok ? Fmt_Float(h, sizeof(h), m.h, 1) : "ERR",
               m.p_ok ? Fmt_Float(p, sizeof(p), m.p, 2) : "ERR",
               DryerCtrl_StateName(s_status.state), (int)s_status.phase,
               s_status.out.comp, s_fan_actual, s_status.out.fan_evap,
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
    Text_Box(0, 92, TFT_WIDTH, "MÁY SẤY TÁCH ẨM", &font_vn16, C_WHITE, C_BLACK, TEXT_CENTER);
    Text_Box(0, 124, TFT_WIDTH, "Đang khởi động…", &font_vn16, C_GRAY, C_BLACK, TEXT_CENTER);

    static const sensors_cfg_t sensors_cfg = {
        .pt100 = &board_pt100, .sht = &board_sht45, .press = &board_press,
        .bus_recover = Board_I2cRecover,
    };
    Sensors_Init(&sensors_cfg);

    uint32_t now = Board_Millis();
    s_vclock = s_last_tick = now;
    s_comp_off_tick = now;                            /* sau khởi động máy nén cũng phải chờ */
    DryerCtrl_Init(&s_ctrl, now);
    FanSpeed_Init(&s_fan, now);
    DryerCtrl_GetStatus(&s_ctrl, &s_status);

    for (uint8_t i = 0; i < PRESET_COUNT; i++) s_preset_names[i] = g_preset_defs[i].name;
    static const ui_config_t ui_cfg = {
        .preset_names = s_preset_names, .preset_count = PRESET_COUNT,
        .tech = &s_tech_params, .process = &s_proc_params, .sim = &s_sim_params, .on_cmd = on_ui_cmd,
        .output_names = s_out_names, .output_pins = s_out_pins, .output_count = RLY_ID_COUNT,
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
