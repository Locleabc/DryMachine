/**
 * @file    dryer_ctrl.c
 */
#include "dryer_ctrl.h"
#include <string.h>

#define SENSOR_FAULT_CYCLES   5         /* số lần Step lỗi liên tiếp mới báo */
#define P_LOW_GRACE_MS        60000UL   /* bỏ qua áp thấp 60 s đầu sau khi bật máy nén */
#define P_HIGH_PRE_RATIO      0.9f      /* ≥ 90% áp cao → ép quạt dàn nóng chạy */

#define MS(sec)   ((uint32_t)((sec) * 1000.0f))
#define CMD_BIT(c) ((uint8_t)(1u << (c)))

static void enter(dryer_ctrl_t *c, dryer_state_t s)
{
    c->state = s;
    c->state_tick = c->now;
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

static void update_demand(dryer_ctrl_t *c, const dryer_params_t *p, const dryer_input_t *in)
{
    bool need, satisfied;
    if (in->hum_ok) {
        need      = (in->temp < p->temp_set - p->temp_hyst) || (in->hum > p->hum_set + p->hum_hyst);
        satisfied = (in->temp >= p->temp_set) && (in->hum <= p->hum_set);
    } else {                                   /* mất cảm biến ẩm: chỉ giữ nhiệt */
        need      = (in->temp < p->temp_set - p->temp_hyst);
        satisfied = (in->temp >= p->temp_set);
    }
    if (!c->comp_demand && need) c->comp_demand = true;
    else if (c->comp_demand && satisfied) c->comp_demand = false;

    c->target_reached = in->hum_ok && (in->hum <= p->hum_set);
}

static bool cond_fan_logic(const dryer_ctrl_t *c, const dryer_params_t *p, const dryer_input_t *in)
{
    if (in->press_ok && in->press >= p->p_high * P_HIGH_PRE_RATIO) return true;
    if (!c->out.comp) return false;
    if (p->cond_fan_mode < 0.5f) return true;                 /* mode 0: theo máy nén */
    if (in->temp > p->temp_set + p->temp_hyst) return true;   /* mode 1: xả nhiệt thừa */
    if (in->temp < p->temp_set) return false;
    return c->out.fan_cond;
}

void DryerCtrl_DefaultParams(dryer_params_t *p)
{
    p->temp_set      = 55.0f;
    p->temp_hyst     = 2.0f;
    p->hum_set       = 15.0f;
    p->hum_hyst      = 3.0f;
    p->temp_max      = 75.0f;
    p->p_high        = 30.0f;   /* TODO: chỉnh theo gas và vị trí cảm biến */
    p->p_low         = 1.0f;
    p->press_enable  = 1.0f;
    p->comp_min_off  = 180.0f;
    p->comp_min_on   = 60.0f;
    p->start_delay   = 10.0f;
    p->fan_post      = 60.0f;
    p->dry_time_h    = 0.0f;
    p->cond_fan_mode = 0.0f;
}

void DryerCtrl_Init(dryer_ctrl_t *c, uint32_t now_ms)
{
    memset(c, 0, sizeof(*c));
    c->now = now_ms;
    c->comp_off_tick = now_ms;       /* sau khi cấp điện vẫn phải chờ min_off */
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
        c->faults = 0;                  /* lỗi còn tồn tại sẽ bị bắt lại ngay */
        c->temp_bad_cnt = c->press_bad_cnt = 0;
        enter(c, DRYER_IDLE);
    }

    check_faults(c, p, in);
    if (c->faults && c->state != DRYER_FAULT) {
        comp_request(c, p, false, true);
        enter(c, DRYER_FAULT);
    }

    switch (c->state) {
    case DRYER_IDLE:
        comp_request(c, p, false, true);
        c->out.fan_evap = c->out.fan_cond = false;
        c->comp_demand = false;
        if (cmd & CMD_BIT(DRYER_CMD_START)) {
            c->run_start = now_ms;
            enter(c, DRYER_STARTING);
        }
        break;

    case DRYER_STARTING:
        c->out.fan_evap = true;
        c->out.fan_cond = false;
        if (cmd & CMD_BIT(DRYER_CMD_STOP)) { enter(c, DRYER_STOPPING); break; }
        if (now_ms - c->state_tick >= MS(p->start_delay)) enter(c, DRYER_RUNNING);
        break;

    case DRYER_RUNNING:
        c->out.fan_evap = true;
        update_demand(c, p, in);
        comp_request(c, p, c->comp_demand, false);
        c->out.fan_cond = cond_fan_logic(c, p, in);
        if ((cmd & CMD_BIT(DRYER_CMD_STOP)) ||
            (p->dry_time_h > 0.0f && (now_ms - c->run_start) >= MS(p->dry_time_h * 3600.0f))) {
            comp_request(c, p, false, true);
            enter(c, DRYER_STOPPING);
        }
        break;

    case DRYER_STOPPING:
    case DRYER_FAULT: {
        comp_request(c, p, false, true);
        bool post = (now_ms - c->state_tick) < MS(p->fan_post);
        c->out.fan_evap = post;
        c->out.fan_cond = post || (in->press_ok && in->press >= p->p_high * P_HIGH_PRE_RATIO);
        if (c->state == DRYER_STOPPING && !post) enter(c, DRYER_IDLE);
        break;
    }
    }

    *out = c->out;
}

void DryerCtrl_GetStatus(const dryer_ctrl_t *c, dryer_status_t *st)
{
    st->state          = c->state;
    st->faults         = c->faults;
    st->warnings       = c->warnings;
    st->out            = c->out;
    st->comp_demand    = c->comp_demand;
    st->target_reached = c->target_reached;

    uint32_t off_for = c->now - c->comp_off_tick;
    st->comp_wait_s = (!c->out.comp && off_for < c->min_off_ms) ? (c->min_off_ms - off_for + 999) / 1000 : 0;

    bool running = (c->state == DRYER_STARTING || c->state == DRYER_RUNNING);
    st->run_s = running ? (c->now - c->run_start) / 1000 : 0;
}

const char *DryerCtrl_StateName(dryer_state_t s)
{
    switch (s) {
    case DRYER_IDLE:     return "DUNG";
    case DRYER_STARTING: return "KHOI DG";
    case DRYER_RUNNING:  return "DANG SAY";
    case DRYER_STOPPING: return "DANG TAT";
    case DRYER_FAULT:    return "LOI";
    default:             return "?";
    }
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
