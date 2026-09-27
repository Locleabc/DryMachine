/**
 * @file    ctrl_dryer.c
 */
#include "ctrl_dryer.h"
#include "bsp_relay.h"
#include "settings.h"

#define SENSOR_FAULT_CYCLES   5        /* số chu kỳ lỗi liên tiếp mới báo (≈1 s) */
#define P_LOW_GRACE_MS        60000UL  /* bỏ qua áp thấp 60 s đầu sau khi bật máy nén */
#define P_HIGH_PRE_RATIO      0.9f     /* ≥ 90% áp cao → ép quạt dàn nóng chạy */

typedef struct {
    ctrl_state_t state;
    uint32_t     state_tick;       /* thời điểm vào trạng thái hiện tại */
    uint32_t     run_start;
    uint32_t     comp_on_tick, comp_off_tick;
    bool         comp, fan_cond, fan_evap;
    bool         comp_demand;
    bool         start_req, stop_req;
    uint16_t     faults, warnings;
    uint8_t      temp_bad_cnt, press_bad_cnt;
    uint32_t     now;
    bool         target_reached;
} ctrl_t;

static ctrl_t c;

#define MS(sec)   ((uint32_t)((sec) * 1000.0f))

static void enter(ctrl_state_t s)
{
    c.state = s;
    c.state_tick = c.now;
}

/* Bật/tắt máy nén có bảo vệ min-on / min-off. force = bỏ qua min-on (dừng khẩn) */
static void comp_request(bool on, bool force)
{
    if (on && !c.comp) {
        if (c.now - c.comp_off_tick >= MS(g_settings.comp_min_off)) {
            c.comp = true;
            c.comp_on_tick = c.now;
        }
    } else if (!on && c.comp) {
        if (force || (c.now - c.comp_on_tick >= MS(g_settings.comp_min_on))) {
            c.comp = false;
            c.comp_off_tick = c.now;
        }
    }
}

static void check_faults(const ctrl_input_t *in)
{
    const settings_t *s = &g_settings;

    /* Cảm biến nhiệt */
    if (!in->temp_ok) { if (c.temp_bad_cnt < 255) c.temp_bad_cnt++; }
    else c.temp_bad_cnt = 0;
    if (c.temp_bad_cnt >= SENSOR_FAULT_CYCLES) c.faults |= FAULT_TEMP_SENSOR;

    if (in->temp_ok && in->temp > s->temp_max) c.faults |= FAULT_OVERTEMP;

    /* Áp suất */
    if (s->press_enable > 0.5f) {
        if (!in->press_ok) { if (c.press_bad_cnt < 255) c.press_bad_cnt++; }
        else c.press_bad_cnt = 0;
        if (c.press_bad_cnt >= SENSOR_FAULT_CYCLES) c.faults |= FAULT_PRESS_SENSOR;

        if (in->press_ok && in->press > s->p_high) c.faults |= FAULT_PRESS_HIGH;
        if (in->press_ok && c.comp && (c.now - c.comp_on_tick > P_LOW_GRACE_MS) &&
            in->press < s->p_low) c.faults |= FAULT_PRESS_LOW;
    }

    /* Cảnh báo ẩm */
    if (!in->hum_ok) c.warnings |= WARN_HUM_SENSOR;
    else c.warnings &= (uint16_t)~WARN_HUM_SENSOR;
}

/* Quyết định có cần chạy máy nén không (có trễ) */
static void update_demand(const ctrl_input_t *in)
{
    const settings_t *s = &g_settings;
    bool need, satisfied;

    if (in->hum_ok) {
        need      = (in->temp < s->temp_set - s->temp_hyst) || (in->hum > s->hum_set + s->hum_hyst);
        satisfied = (in->temp >= s->temp_set) && (in->hum <= s->hum_set);
    } else {
        /* Mất cảm biến ẩm: chỉ giữ nhiệt độ */
        need      = (in->temp < s->temp_set - s->temp_hyst);
        satisfied = (in->temp >= s->temp_set);
    }

    if (!c.comp_demand && need) c.comp_demand = true;
    else if (c.comp_demand && satisfied) c.comp_demand = false;

    c.target_reached = in->hum_ok && (in->hum <= s->hum_set);
}

static bool cond_fan_logic(const ctrl_input_t *in)
{
    const settings_t *s = &g_settings;

    /* Áp gần ngưỡng cao → luôn chạy quạt dàn nóng */
    if (in->press_ok && in->press >= s->p_high * P_HIGH_PRE_RATIO) return true;
    if (!c.comp) return false;

    if (s->cond_fan_mode < 0.5f) return true;              /* mode 0: theo máy nén */

    /* mode 1: xả nhiệt thừa khi buồng quá nóng */
    if (in->temp > s->temp_set + s->temp_hyst) return true;
    if (in->temp < s->temp_set) return false;
    return c.fan_cond;                                     /* giữ trạng thái trong vùng trễ */
}

void Ctrl_Init(uint32_t now_ms)
{
    c = (ctrl_t){0};
    c.now = now_ms;
    c.comp_off_tick = now_ms;   /* sau khi cấp điện vẫn phải chờ min_off */
    enter(CTRL_IDLE);
    Relay_AllOff();
}

void Ctrl_Start(void) { c.start_req = true; }
void Ctrl_Stop(void)  { c.stop_req  = true; }

bool Ctrl_ResetFault(void)
{
    if (c.state != CTRL_FAULT) return true;
    c.faults = 0;               /* xoá; nếu lỗi vẫn còn sẽ bị bắt lại ở chu kỳ sau */
    c.temp_bad_cnt = c.press_bad_cnt = 0;
    enter(CTRL_IDLE);
    return true;
}

void Ctrl_Update(const ctrl_input_t *in, uint32_t now_ms)
{
    const settings_t *s = &g_settings;
    c.now = now_ms;

    check_faults(in);
    if (c.faults && c.state != CTRL_FAULT) {
        comp_request(false, true);
        enter(CTRL_FAULT);
    }

    switch (c.state) {
    case CTRL_IDLE:
        comp_request(false, true);
        c.fan_evap = c.fan_cond = false;
        c.comp_demand = false;
        if (c.start_req) {
            c.run_start = now_ms;
            enter(CTRL_STARTING);
        }
        break;

    case CTRL_STARTING:
        c.fan_evap = true;
        c.fan_cond = false;
        if (c.stop_req) { enter(CTRL_STOPPING); break; }
        if (now_ms - c.state_tick >= MS(s->start_delay)) enter(CTRL_RUNNING);
        break;

    case CTRL_RUNNING:
        c.fan_evap = true;
        update_demand(in);
        comp_request(c.comp_demand, false);
        c.fan_cond = cond_fan_logic(in);
        if (c.stop_req) { comp_request(false, true); enter(CTRL_STOPPING); break; }
        if (s->dry_time_h > 0.0f && (now_ms - c.run_start) >= MS(s->dry_time_h * 3600.0f)) {
            comp_request(false, true);
            enter(CTRL_STOPPING);
        }
        break;

    case CTRL_STOPPING:
    case CTRL_FAULT: {
        comp_request(false, true);
        bool post = (now_ms - c.state_tick) < MS(s->fan_post);
        c.fan_evap = post;
        c.fan_cond = post || (in->press_ok && in->press >= s->p_high * P_HIGH_PRE_RATIO);
        if (c.state == CTRL_STOPPING && !post) enter(CTRL_IDLE);
        break;
    }
    }
    c.start_req = c.stop_req = false;

    Relay_Set(RELAY_COMP,     c.comp);
    Relay_Set(RELAY_FAN_COND, c.fan_cond);
    Relay_Set(RELAY_FAN_EVAP, c.fan_evap);
}

void Ctrl_GetStatus(ctrl_status_t *st)
{
    st->state       = c.state;
    st->faults      = c.faults;
    st->warnings    = c.warnings;
    st->comp        = c.comp;
    st->fan_cond    = c.fan_cond;
    st->fan_evap    = c.fan_evap;
    st->comp_demand = c.comp_demand;
    st->target_reached = c.target_reached;

    uint32_t min_off = MS(g_settings.comp_min_off);
    uint32_t off_for = c.now - c.comp_off_tick;
    st->comp_wait_s = (!c.comp && off_for < min_off) ? (min_off - off_for + 999) / 1000 : 0;

    bool running = (c.state == CTRL_STARTING || c.state == CTRL_RUNNING);
    st->run_s = running ? (c.now - c.run_start) / 1000 : 0;
}

const char *Ctrl_StateName(ctrl_state_t s)
{
    switch (s) {
    case CTRL_IDLE:     return "DUNG   ";
    case CTRL_STARTING: return "KHOI DG";
    case CTRL_RUNNING:  return "DANG SAY";
    case CTRL_STOPPING: return "DANG TAT";
    case CTRL_FAULT:    return "LOI    ";
    default:            return "?";
    }
}

const char *Ctrl_FaultText(uint16_t f)
{
    if (f & FAULT_PRESS_HIGH)   return "LOI: AP SUAT CAO";
    if (f & FAULT_PRESS_LOW)    return "LOI: AP SUAT THAP";
    if (f & FAULT_OVERTEMP)     return "LOI: QUA NHIET";
    if (f & FAULT_TEMP_SENSOR)  return "LOI: CAM BIEN NHIET";
    if (f & FAULT_PRESS_SENSOR) return "LOI: CAM BIEN AP";
    return "";
}
