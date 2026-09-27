/* Test logic control/dryer_ctrl trên PC – KHÔNG cần HAL (chứng minh tầng control độc lập).
 * Chạy: gcc -std=c11 -IApp/control tools/host_check/test_ctrl.c App/control/dryer_ctrl.c -o t && ./t
 */
#include "dryer_ctrl.h"
#include <stdio.h>

static int fails;
#define EXPECT(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); fails++; } else printf("ok  : %s\n", msg); } while (0)

static dryer_ctrl_t   c;
static dryer_params_t p;
static dryer_output_t out;
static dryer_status_t st;
static uint32_t now;

static void run(const dryer_input_t *in, uint32_t ms)
{
    for (uint32_t t = 0; t < ms; t += 200) { now += 200; DryerCtrl_Step(&c, &p, in, now, &out); }
    DryerCtrl_GetStatus(&c, &st);
}

int main(void)
{
    DryerCtrl_DefaultParams(&p);
    dryer_input_t in = { true, 30.0f, true, 60.0f, true, 15.0f };
    now = 1000;
    DryerCtrl_Init(&c, now);

    run(&in, 1000);
    EXPECT(st.state == DRYER_IDLE && !out.comp && !out.fan_evap, "IDLE: tat ca OFF");

    DryerCtrl_Command(&c, DRYER_CMD_TOGGLE); run(&in, 400);
    EXPECT(st.state == DRYER_STARTING && out.fan_evap && !out.comp, "TOGGLE -> STARTING, quat lanh chay truoc");

    run(&in, 10000);
    EXPECT(st.state == DRYER_RUNNING, "RUNNING sau start_delay");
    EXPECT(!out.comp && st.comp_wait_s > 0, "May nen cho min_off sau cap dien");

    run(&in, 180000);
    EXPECT(out.comp && out.fan_cond, "May nen + quat nong chay (mode 0)");

    in.temp = 56.0f; in.hum = 14.0f; run(&in, 61000);
    EXPECT(!out.comp && st.target_reached, "Dat nhiet+am -> tat may nen");

    in.hum = 16.0f; run(&in, 1000);
    EXPECT(!out.comp, "Trong vung tre am -> van tat");
    in.hum = 19.0f; run(&in, 1000);
    EXPECT(!out.comp && st.comp_demand, "Can chay nhung dang cho min_off");
    run(&in, 180000);
    EXPECT(out.comp, "Het min_off -> may nen chay lai");

    in.press = 31.0f; run(&in, 200);
    EXPECT(st.state == DRYER_FAULT && !out.comp && (st.faults & DRYER_FAULT_PRESS_HIGH), "Ap cao -> FAULT, tat may nen ngay");
    EXPECT(out.fan_cond, "FAULT ap cao: quat nong van chay");
    in.press = 15.0f; run(&in, 61000);
    EXPECT(!out.fan_evap && !out.fan_cond, "FAULT: quat tat sau fan_post");
    DryerCtrl_Command(&c, DRYER_CMD_RESET_FAULT); run(&in, 400);
    EXPECT(st.state == DRYER_IDLE && st.faults == 0, "Reset loi -> IDLE");

    in.temp_ok = false; run(&in, 2000);
    EXPECT(st.state == DRYER_FAULT && (st.faults & DRYER_FAULT_TEMP_SENSOR), "Mat cam bien nhiet -> FAULT");
    in.temp_ok = true; DryerCtrl_Command(&c, DRYER_CMD_RESET_FAULT); run(&in, 400);

    in.hum_ok = false; in.temp = 40.0f; DryerCtrl_Command(&c, DRYER_CMD_START); run(&in, 200000);
    EXPECT(st.state == DRYER_RUNNING && (st.warnings & DRYER_WARN_HUM_SENSOR) && out.comp, "Mat CB am: chay theo nhiet + canh bao");

    DryerCtrl_Command(&c, DRYER_CMD_TOGGLE); run(&in, 400);
    EXPECT(st.state == DRYER_STOPPING && !out.comp && out.fan_evap, "TOGGLE khi chay -> STOPPING, quat chay them");
    run(&in, 61000);
    EXPECT(st.state == DRYER_IDLE && !out.fan_evap, "Het fan_post -> IDLE");

    p.dry_time_h = 0.1f; in.hum_ok = true; DryerCtrl_Command(&c, DRYER_CMD_START); run(&in, 361000);
    EXPECT(st.state == DRYER_STOPPING || st.state == DRYER_IDLE, "Het thoi gian say -> tu dung");

    printf("\n%s (%d loi)\n", fails ? "CO LOI" : "TAT CA PASS", fails);
    return fails;
}
