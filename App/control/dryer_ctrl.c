/**
 * @file    dryer_ctrl.c
 */
#include "dryer_ctrl.h"
#include <string.h>

#define SENSOR_FAULT_CYCLES   5         /* số lần Step lỗi liên tiếp mới báo */
#define P_LOW_GRACE_MS        60000UL   /* bỏ qua áp thấp 60 s đầu sau khi bật máy nén */
#define P_HIGH_PRE_RATIO      0.9f      /* ≥ 90% áp cao → quạt dàn nóng luôn chạy */

#define MS(sec)    ((uint32_t)((sec) * 1000.0f))
#define CMD_BIT(c) ((uint8_t)(1u << (c)))

/* ---------------- tiện ích ---------------- */
static uint8_t fan_lvl(float v)
{
    int l = (int)(v + 0.5f);
    if (l < 1) l = 1;
    if (l > DRYER_FAN_LEVELS) l = DRYER_FAN_LEVELS;
    return (uint8_t)l;
}

static void enter(dryer_ctrl_t *c, dryer_state_t s)
{
    c->state = s;
    c->state_tick = c->now;
}

static void set_phase(dryer_ctrl_t *c, dryer_phase_t ph, uint32_t len_ms)
{
    c->phase = ph;
    c->phase_tick = c->now;
    c->phase_len_ms = len_ms;
    c->comp_demand = false;
}

static void comp_request(dryer_ctrl_t *c, const dryer_params_t *p, bool on, bool force)
{
    if (on && !c->out.comp) {
        if (c->now - c->comp_off_tick >= MS(p->comp_min_off)) {
            c->out.comp = true;
            c->comp_on_tick = c->now;
        }
    } else if (!on && c->out.comp) {
        if (force || (c->now - c->comp_on_tick >= MS(p->comp_min_on))) {
            c->out.comp = false;
            c->comp_off_tick = c->now;
        }
    }
}

/* Giữ nhiệt độ: bật khi < đặt - trễ, tắt khi ≥ đặt */
static void thermostat(dryer_ctrl_t *c, const dryer_params_t *p, const dryer_input_t *in)
{
    if (!c->comp_demand && in->temp < p->temp_set - p->temp_hyst) c->comp_demand = true;
    else if (c->comp_demand && in->temp >= p->temp_set)          c->comp_demand = false;
}

static void check_faults(dryer_ctrl_t *c, const dryer_params_t *p, const dryer_input_t *in)
{
    if (!in->temp_ok) { if (c->temp_bad_cnt < 255) c->temp_bad_cnt++; }
    else c->temp_bad_cnt = 0;
    if (c->temp_bad_cnt >= SENSOR_FAULT_CYCLES) c->faults |= DRYER_FAULT_TEMP_SENSOR;

    if (in->temp_ok && in->temp > p->temp_max) c->faults |= DRYER_FAULT_OVERTEMP;

    if (p->press_enable > 0.5f) {
        if (!in->press_ok) { if (c->press_bad_cnt < 255) c->press_bad_cnt++; }
        else c->press_bad_cnt = 0;
        if (c->press_bad_cnt >= SENSOR_FAULT_CYCLES) c->faults |= DRYER_FAULT_PRESS_SENSOR;

        if (in->press_ok && in->press > p->p_high) c->faults |= DRYER_FAULT_PRESS_HIGH;
        if (in->press_ok && c->out.comp && (c->now - c->comp_on_tick > P_LOW_GRACE_MS) &&
            in->press < p->p_low) c->faults |= DRYER_FAULT_PRESS_LOW;
    }

    if (!in->hum_ok) c->warnings |= DRYER_WARN_HUM_SENSOR;
    else c->warnings &= (uint16_t)~DRYER_WARN_HUM_SENSOR;
}

static bool press_near_high(const dryer_params_t *p, const dryer_input_t *in)
{
    return in->press_ok && in->press >= p->p_high * P_HIGH_PRE_RATIO;
}

/* Vào giai đoạn làm mát (máy nén tắt) */
static void go_cool(dryer_ctrl_t *c, const dryer_params_t *p)
{
    comp_request(c, p, false, true);
    set_phase(c, c->manual ? PH_GD5 : PH_AUTO_COOL, 0);
}

/* ---------------- các giai đoạn ---------------- */
static void run_phase(dryer_ctrl_t *c, const dryer_params_t *p, const dryer_input_t *in)
{
    uint32_t in_phase = c->now - c->phase_tick;
    bool comp_on = false;

    switch (c->phase) {
    /* ----- tự động ----- */
    case PH_AUTO_DRY:
        c->out.fan_level = fan_lvl(p->auto_fan);
        if (!in->hum_ok || in->hum <= p->hum_set) {   /* đạt ẩm (hoặc mất cảm biến ẩm) */
            set_phase(c, PH_AUTO_HOLD, 0);
            comp_request(c, p, false, false);
            break;
        }
        comp_on = true;
        break;
    case PH_AUTO_HOLD:
        c->out.fan_level = fan_lvl(p->auto_fan);
        thermostat(c, p, in);
        comp_on = c->comp_demand;
        break;
    case PH_AUTO_COOL:
    case PH_GD5:
        c->out.fan_level = fan_lvl(c->phase == PH_GD5 ? p->stage_fan[4] : p->auto_fan);
        comp_request(c, p, false, true);
        if (in->temp_ok && in->temp <= p->end_temp) {   /* nguội đủ → kết thúc */
            c->finished = true;
            c->out.fan_level = 0;
            c->out.fan_evap = false;
            set_phase(c, PH_NONE, 0);
            enter(c, DRYER_IDLE);
        }
        return;

    /* ----- thủ công ----- */
    case PH_GD1:
        c->out.fan_level = fan_lvl(p->stage_fan[0]);
        if (in->temp >= p->temp_set) { set_phase(c, PH_GD2, 0); break; }
        comp_on = true;
        break;
    case PH_GD2:
        c->out.fan_level = fan_lvl(p->stage_fan[1]);
        if (!in->hum_ok || in->hum <= p->hum_set) { set_phase(c, PH_GD3, MS(p->gd3_min * 60.0f)); break; }
        thermostat(c, p, in);
        comp_on = c->comp_demand;
        break;
    case PH_GD3:
    case PH_GD4:
        c->out.fan_level = fan_lvl(p->stage_fan[c->phase == PH_GD3 ? 2 : 3]);
        if (in_phase >= c->phase_len_ms) {
            if (c->phase == PH_GD3) set_phase(c, PH_GD4, MS(p->gd4_min * 60.0f));
            else                    go_cool(c, p);
            break;
        }
        thermostat(c, p, in);
        comp_on = c->comp_demand;
        break;
    default:
        break;
    }

    if (c->phase == PH_AUTO_COOL || c->phase == PH_GD5) return;   /* vừa chuyển sang làm mát */
    comp_request(c, p, comp_on, false);

    /* Tự động: hết thời gian sấy → làm mát */
    if (!c->manual && p->dry_time_h > 0.0f && (c->now - c->run_start) >= MS(p->dry_time_h * 3600.0f)) {
        go_cool(c, p);
    }
}

/* ---------------- API ---------------- */
void DryerCtrl_DefaultParams(dryer_params_t *p)
{
    memset(p, 0, sizeof(*p));
    p->temp_set      = 55.0f;
    p->temp_hyst     = 2.0f;
    p->hum_set       = 15.0f;
    p->temp_recover  = 30.0f;   /* quá nhiệt: nguội tới 30 °C mới hết lỗi */
    p->temp_max      = 75.0f;
    p->p_high        = 30.0f;   /* TODO: chỉnh theo gas và vị trí cảm biến */
    p->p_low         = 1.0f;
    p->press_enable  = 1.0f;
    p->comp_min_off  = 60.0f;   /* chờ bật lại máy nén */
    p->comp_min_on   = 30.0f;
    p->start_delay   = 60.0f;   /* quạt chạy 60 s rồi mới bật máy nén */
    p->fan_post      = 60.0f;
    p->dry_time_h    = 0.0f;
    p->mode          = 0.0f;    /* tự động */
    p->auto_fan      = 3.0f;
    p->stage_fan[0]  = 3.0f;
    p->stage_fan[1]  = 3.0f;
    p->stage_fan[2]  = 2.0f;
    p->stage_fan[3]  = 2.0f;
    p->stage_fan[4]  = 5.0f;
    p->gd3_min       = 120.0f;
    p->gd4_min       = 60.0f;
    p->end_temp      = 40.0f;
}

void DryerCtrl_Init(dryer_ctrl_t *c, uint32_t now_ms)
{
    memset(c, 0, sizeof(*c));
    c->now = now_ms;
    c->comp_off_tick = now_ms;       /* sau khi cấp điện vẫn phải chờ comp_min_off */
    enter(c, DRYER_IDLE);
}

void DryerCtrl_Command(dryer_ctrl_t *c, dryer_cmd_t cmd)
{
    c->pending_cmd |= CMD_BIT(cmd);
}

void DryerCtrl_Step(dryer_ctrl_t *c, const dryer_params_t *p, const dryer_input_t *in,
                    uint32_t now_ms, dryer_output_t *out)
{
    c->now = now_ms;
    c->min_off_ms = MS(p->comp_min_off);

    uint8_t cmd = c->pending_cmd;
    c->pending_cmd = 0;
    if (cmd & CMD_BIT(DRYER_CMD_TOGGLE)) {
        if (c->state == DRYER_IDLE) cmd |= CMD_BIT(DRYER_CMD_START);
        else if (c->state == DRYER_STARTING || c->state == DRYER_RUNNING) cmd |= CMD_BIT(DRYER_CMD_STOP);
    }
    if ((cmd & CMD_BIT(DRYER_CMD_RESET_FAULT)) && c->state == DRYER_FAULT) {
        /* quá nhiệt: chưa nguội (còn đo được và > nhiệt độ hết lỗi) thì không cho reset */
        bool hot = (c->faults & DRYER_FAULT_OVERTEMP) && in->temp_ok && in->temp > DryerCtrl_RecoverTemp(p);
        if (!hot) {
            c->faults = 0;
            c->temp_bad_cnt = c->press_bad_cnt = 0;
            enter(c, DRYER_IDLE);
        }
    }

    check_faults(c, p, in);
    if (c->faults && c->state != DRYER_FAULT) {
        comp_request(c, p, false, true);
        set_phase(c, PH_NONE, 0);
        enter(c, DRYER_FAULT);
    }

    switch (c->state) {
    case DRYER_IDLE:
        comp_request(c, p, false, true);
        c->out.fan_evap = false;
        c->out.fan_level = 0;
        c->comp_demand = false;
        if (cmd & CMD_BIT(DRYER_CMD_START)) {
            c->manual = (p->mode > 0.5f);
            c->finished = false;
            c->run_start = now_ms;
            set_phase(c, PH_NONE, 0);
            enter(c, DRYER_STARTING);
        }
        break;

    case DRYER_STARTING:                      /* quạt chạy trước, máy nén chưa bật */
        c->out.fan_evap  = true;
        c->out.fan_level = fan_lvl(c->manual ? p->stage_fan[0] : p->auto_fan);
        if (cmd & CMD_BIT(DRYER_CMD_STOP)) { enter(c, DRYER_STOPPING); break; }
        if (now_ms - c->state_tick >= MS(p->start_delay)) {
            set_phase(c, c->manual ? PH_GD1 : PH_AUTO_DRY, 0);
            enter(c, DRYER_RUNNING);
        }
        break;

    case DRYER_RUNNING:
        c->out.fan_evap = true;
        if (cmd & CMD_BIT(DRYER_CMD_STOP)) {
            comp_request(c, p, false, true);
            set_phase(c, PH_NONE, 0);
            enter(c, DRYER_STOPPING);
            break;
        }
        run_phase(c, p, in);
        break;

    case DRYER_STOPPING:
    case DRYER_FAULT: {
        comp_request(c, p, false, true);
        if (c->state == DRYER_FAULT && (c->faults & DRYER_FAULT_OVERTEMP)) {
            /* quá nhiệt: xả nhiệt tối đa liên tục tới khi nguội */
            c->out.fan_evap  = true;
            c->out.fan_level = DRYER_FAN_LEVELS;
            if (in->temp_ok && in->temp <= DryerCtrl_RecoverTemp(p)) {
                c->faults &= (uint16_t)~DRYER_FAULT_OVERTEMP;
                if (c->faults == 0) {                       /* chỉ có quá nhiệt → hết lỗi, máy dừng */
                    c->out.fan_evap  = false;
                    c->out.fan_level = 0;
                    enter(c, DRYER_IDLE);
                } else {
                    enter(c, DRYER_FAULT);                  /* còn lỗi khác: quạt chạy thêm fan_post như thường */
                }
            }
            break;
        }
        bool post = (now_ms - c->state_tick) < MS(p->fan_post);
        c->out.fan_evap = post;
        if (press_near_high(p, in) || (c->faults & DRYER_FAULT_PRESS_HIGH && post)) {
            c->out.fan_level = DRYER_FAN_LEVELS;                 /* áp cao: xả nhiệt tối đa */
        } else if (post) {
            if (c->out.fan_level == 0) c->out.fan_level = DRYER_FAN_LEVELS;
        } else {
            c->out.fan_level = 0;
        }
        if (c->state == DRYER_STOPPING && !post) enter(c, DRYER_IDLE);
        break;
    }
    }

    /* áp gần ngưỡng cao khi máy chạy → quạt dàn nóng tối thiểu cấp 1 */
    if ((c->state == DRYER_RUNNING || c->state == DRYER_STARTING) && press_near_high(p, in) &&
        c->out.fan_level == 0) c->out.fan_level = 1;

    c->target_reached = in->hum_ok && (in->hum <= p->hum_set);
    *out = c->out;
}

void DryerCtrl_GetStatus(const dryer_ctrl_t *c, dryer_status_t *st)
{
    st->state          = c->state;
    st->phase          = c->phase;
    st->manual         = c->manual;
    st->faults         = c->faults;
    st->warnings       = c->warnings;
    st->out            = c->out;
    st->comp_demand    = c->comp_demand ||
                         (c->state == DRYER_RUNNING && (c->phase == PH_AUTO_DRY || c->phase == PH_GD1));
    st->target_reached = c->target_reached;
    st->finished       = c->finished;

    uint32_t off_for = c->now - c->comp_off_tick;
    st->comp_wait_s = (!c->out.comp && off_for < c->min_off_ms) ? (c->min_off_ms - off_for + 999) / 1000 : 0;

    bool active = (c->state == DRYER_STARTING || c->state == DRYER_RUNNING);
    st->run_s   = active ? (c->now - c->run_start) / 1000 : 0;
    st->phase_s = (c->state == DRYER_RUNNING) ? (c->now - c->phase_tick) / 1000 : 0;
    st->phase_left_s = 0;
    if (c->state == DRYER_RUNNING && (c->phase == PH_GD3 || c->phase == PH_GD4)) {
        uint32_t el = c->now - c->phase_tick;
        st->phase_left_s = (el < c->phase_len_ms) ? (c->phase_len_ms - el + 999) / 1000 : 0;
    }
}

const char *DryerCtrl_StateName(dryer_state_t s)
{
    switch (s) {
    case DRYER_IDLE:     return "Đang dừng";
    case DRYER_STARTING: return "Khởi động";
    case DRYER_RUNNING:  return "Đang sấy";
    case DRYER_STOPPING: return "Đang tắt";
    case DRYER_FAULT:    return "Lỗi";
    default:             return "?";
    }
}

const char *DryerCtrl_PhaseName(dryer_phase_t ph)
{
    switch (ph) {
    case PH_AUTO_DRY:  return "Tự động – hút ẩm";
    case PH_AUTO_HOLD: return "Tự động – giữ nhiệt";
    case PH_AUTO_COOL: return "Tự động – làm mát";
    case PH_GD1:       return "GĐ1 – gia nhiệt";
    case PH_GD2:       return "GĐ2 – hút ẩm";
    case PH_GD3:       return "GĐ3 – giữ nhiệt";
    case PH_GD4:       return "GĐ4 – giữ nhiệt";
    case PH_GD5:       return "GĐ5 – làm mát";
    default:           return "";
    }
}

float DryerCtrl_RecoverTemp(const dryer_params_t *p)
{
    float r = p->temp_recover;
    if (r > p->temp_max - 5.0f) r = p->temp_max - 5.0f;
    return r;
}

bool DryerCtrl_OvertempCooling(const dryer_ctrl_t *c)
{
    return c->state == DRYER_FAULT && (c->faults & DRYER_FAULT_OVERTEMP);
}

const char *DryerCtrl_FaultText(uint16_t f)
{
    if (f & DRYER_FAULT_PRESS_HIGH)   return "Áp suất cao";
    if (f & DRYER_FAULT_PRESS_LOW)    return "Áp suất thấp";
    if (f & DRYER_FAULT_OVERTEMP)     return "Quá nhiệt";
    if (f & DRYER_FAULT_TEMP_SENSOR)  return "Mất cảm biến nhiệt";
    if (f & DRYER_FAULT_PRESS_SENSOR) return "Mất cảm biến áp";
    return "";
}
