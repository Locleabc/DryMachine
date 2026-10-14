/**
 * @file    dryer_ctrl.c
 */
#include "dryer_ctrl.h"
#include <string.h>

#define SENSOR_FAULT_CYCLES   5         /* số lần Step lỗi liên tiếp mới báo */
#define HUM_HOLD_HYST         1.0f      /* %RH: GĐ3/GĐ4 ẩm vượt mức + trễ này mới đếm lại */

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
    c->hold_counting = false;
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

    /* nhiệt độ dàn lạnh = thấp nhất trong các NTC đọc được */
    bool all_ok = true;
    c->evap_ok = false;
    for (uint8_t i = 0; i < DRYER_EVAP_COUNT; i++) {
        if (!in->evap_ok[i]) { all_ok = false; continue; }
        if (!c->evap_ok || in->evap[i] < c->evap_min) c->evap_min = in->evap[i];
        c->evap_ok = true;
    }
    if (p->evap_protect > 0.5f && !all_ok) c->warnings |= DRYER_WARN_EVAP_SENSOR;
    else c->warnings &= (uint16_t)~DRYER_WARN_EVAP_SENSOR;

    if (!in->hum_ok) c->warnings |= DRYER_WARN_HUM_SENSOR;
    else c->warnings &= (uint16_t)~DRYER_WARN_HUM_SENSOR;
}

/* Chống đóng băng dàn lạnh: chỉ đếm khi máy nén đang chạy; xả đá → máy nén tắt tới khi ấm lại */
static void defrost_update(dryer_ctrl_t *c, const dryer_params_t *p)
{
    if (p->evap_protect < 0.5f) { c->defrost = false; c->freezing = false; return; }
    if (c->defrost) {
        if (!c->evap_ok || c->evap_min >= p->evap_resume) c->defrost = false;
        return;
    }
    if (c->out.comp && c->evap_ok && c->evap_min <= p->evap_freeze) {
        if (!c->freezing) { c->freezing = true; c->freeze_tick = c->now; }
        if (c->now - c->freeze_tick >= MS(p->evap_freeze_s)) {
            c->defrost = true;
            c->freezing = false;
            if (c->defrost_count < 0xFFFF) c->defrost_count++;
        }
    } else {
        c->freezing = false;
    }
}

/* Vào giai đoạn làm mát (máy nén tắt) */
static void go_cool(dryer_ctrl_t *c, const dryer_params_t *p)
{
    comp_request(c, p, false, true);
    set_phase(c, PH_GD5, 0);
}

/* GD3/GD4: độ ẩm ≤ target liên tục len_ms → true. Ẩm > target + trễ thì đếm lại.
 * Mất cảm biến ẩm: coi như đạt (chỉ tính thời gian) để chu trình không bị kẹt. */
static bool hum_hold_done(dryer_ctrl_t *c, const dryer_input_t *in, float target, uint32_t len_ms)
{
    if (!in->hum_ok || in->hum <= target) {
        if (!c->hold_counting) { c->hold_counting = true; c->hold_tick = c->now; }
    } else if (in->hum > target + HUM_HOLD_HYST) {
        c->hold_counting = false;
    }
    return c->hold_counting && (c->now - c->hold_tick >= len_ms);
}

/* ---------------- các giai đoạn ---------------- */
static void run_phase(dryer_ctrl_t *c, const dryer_params_t *p, const dryer_input_t *in)
{
    bool comp_on = false;

    switch (c->phase) {
    case PH_GD5:
        c->out.fan_level = fan_lvl(p->stage_fan[4]);
        comp_request(c, p, false, true);
        if (in->temp_ok && in->temp <= p->end_temp) {   /* nguội đủ → kết thúc */
            c->finished = true;
            c->out.fan_level = 0;
            c->out.fan_evap = false;
            set_phase(c, PH_NONE, 0);
            enter(c, DRYER_IDLE);
        }
        return;
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
    case PH_GD4: {
        bool gd3 = (c->phase == PH_GD3);
        c->out.fan_level = fan_lvl(p->stage_fan[gd3 ? 2 : 3]);
        if (hum_hold_done(c, in, gd3 ? p->hum_gd3 : p->hum_gd4, c->phase_len_ms)) {
            if (gd3) set_phase(c, PH_GD4, MS(p->gd4_min * 60.0f));
            else     go_cool(c, p);
            break;
        }
        thermostat(c, p, in);
        comp_on = c->comp_demand;
        break;
    }
    default:
        break;
    }

    if (c->phase == PH_GD5) return;                               /* vừa chuyển sang làm mát */
    defrost_update(c, p);
    if (c->defrost) comp_request(c, p, false, true);              /* xả đá: tắt ngay, bỏ qua chạy tối thiểu */
    else            comp_request(c, p, comp_on, false);
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
    p->evap_freeze   = -2.0f;   /* dàn lạnh ≤ −2 °C … */
    p->evap_freeze_s = 60.0f;   /* … liên tục 60 s → xả đá */
    p->evap_resume   = 5.0f;    /* dàn lạnh ≥ 5 °C → chạy lại */
    p->evap_protect  = 1.0f;
    p->comp_min_off  = 60.0f;   /* chờ bật lại máy nén */
    p->comp_min_on   = 30.0f;
    p->start_delay   = 60.0f;   /* quạt chạy 60 s rồi mới bật máy nén */
    p->fan_post      = 60.0f;
    p->hum_gd3       = 12.0f;   /* GĐ3: ẩm ≤ 12 % liên tục gd3_min */
    p->hum_gd4       = 10.0f;   /* GĐ4: ẩm ≤ 10 % liên tục gd4_min */
    p->stage_fan[0]  = 3.0f;
    p->stage_fan[1]  = 3.0f;
    p->stage_fan[2]  = 2.0f;
    p->stage_fan[3]  = 2.0f;
    p->stage_fan[4]  = 5.0f;
    p->gd3_min       = 60.0f;
    p->gd4_min       = 30.0f;
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
            c->temp_bad_cnt = 0;
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
            c->finished = false;
            c->run_start = now_ms;
            set_phase(c, PH_NONE, 0);
            enter(c, DRYER_STARTING);
        }
        break;

    case DRYER_STARTING:                      /* quạt chạy trước, máy nén chưa bật */
        c->out.fan_evap  = true;
        c->out.fan_level = fan_lvl(p->stage_fan[0]);
        if (cmd & CMD_BIT(DRYER_CMD_STOP)) { enter(c, DRYER_STOPPING); break; }
        if (now_ms - c->state_tick >= MS(p->start_delay)) {
            set_phase(c, PH_GD1, 0);
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
        if (post) {
            if (c->out.fan_level == 0) c->out.fan_level = DRYER_FAN_LEVELS;
        } else {
            c->out.fan_level = 0;
        }
        if (c->state == DRYER_STOPPING && !post) enter(c, DRYER_IDLE);
        break;
    }
    }

    if (c->state != DRYER_RUNNING) { c->defrost = false; c->freezing = false; }   /* chỉ xả đá khi đang sấy */
    if (c->defrost) c->warnings |= DRYER_WARN_DEFROST;
    else            c->warnings &= (uint16_t)~DRYER_WARN_DEFROST;

    c->target_reached = in->hum_ok && (in->hum <= p->hum_set);
    *out = c->out;
}

void DryerCtrl_GetStatus(const dryer_ctrl_t *c, dryer_status_t *st)
{
    st->state          = c->state;
    st->phase          = c->phase;
    st->faults         = c->faults;
    st->warnings       = c->warnings;
    st->out            = c->out;
    st->comp_demand    = c->comp_demand ||
                         (c->state == DRYER_RUNNING && c->phase == PH_GD1);
    st->target_reached = c->target_reached;
    st->defrost        = c->defrost;
    st->evap_ok        = c->evap_ok;
    st->evap_min       = c->evap_min;
    st->defrost_count  = c->defrost_count;
    st->finished       = c->finished;

    uint32_t off_for = c->now - c->comp_off_tick;
    st->comp_wait_s = (!c->out.comp && off_for < c->min_off_ms) ? (c->min_off_ms - off_for + 999) / 1000 : 0;

    bool active = (c->state == DRYER_STARTING || c->state == DRYER_RUNNING);
    st->run_s   = active ? (c->now - c->run_start) / 1000 : 0;
    st->phase_s = (c->state == DRYER_RUNNING) ? (c->now - c->phase_tick) / 1000 : 0;
    st->phase_left_s = 0;
    st->hold_counting = false;
    if (c->state == DRYER_RUNNING && (c->phase == PH_GD3 || c->phase == PH_GD4)) {
        uint32_t el = c->hold_counting ? c->now - c->hold_tick : 0;
        st->phase_left_s  = (el < c->phase_len_ms) ? (c->phase_len_ms - el + 999) / 1000 : 0;
        st->hold_counting = c->hold_counting;
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
    case PH_GD1:       return "GĐ1 – gia nhiệt";
    case PH_GD2:       return "GĐ2 – hút ẩm";
    case PH_GD3:       return "GĐ3 – giữ ẩm";
    case PH_GD4:       return "GĐ4 – giữ ẩm";
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
    if (f & DRYER_FAULT_OVERTEMP)     return "Quá nhiệt";
    if (f & DRYER_FAULT_TEMP_SENSOR)  return "Mất cảm biến nhiệt";
    if (f & DRYER_FAULT_OLD_PRESS)    return "Áp suất (bản cũ)";
    return "";
}
