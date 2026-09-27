/* Test logic ctrl_dryer trên PC.
 * Chạy: gcc -std=c11 -Itools/host_check -IApp/Inc tools/host_check/test_ctrl.c \
 *        App/Src/ctrl_dryer.c App/Src/settings.c App/Src/bsp_relay.c -lm -o /tmp/t && /tmp/t
 */
#include "main.h"
#include "ctrl_dryer.h"
#include "settings.h"
#include <stdio.h>
#include <string.h>

GPIO_TypeDef GPIOA_s, GPIOB_s, GPIOC_s;
static uint8_t fake_flash[1024];
void HAL_GPIO_WritePin(GPIO_TypeDef *p, uint16_t n, GPIO_PinState s) { (void)p; (void)n; (void)s; }
/* settings.c đọc SETTINGS_FLASH_ADDR – trên PC không đọc được, nên test gọi Settings_Default() */
HAL_StatusTypeDef HAL_FLASH_Unlock(void) { return HAL_OK; }
HAL_StatusTypeDef HAL_FLASH_Lock(void) { return HAL_OK; }
HAL_StatusTypeDef HAL_FLASHEx_Erase(FLASH_EraseInitTypeDef *e, uint32_t *x) { (void)e; (void)x; return HAL_OK; }
HAL_StatusTypeDef HAL_FLASH_Program(uint32_t t, uint32_t a, uint64_t d) { (void)t; (void)a; (void)d; (void)fake_flash; return HAL_OK; }

static int fails;
#define EXPECT(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); fails++; } else printf("ok  : %s\n", msg); } while (0)

static uint32_t now;
static ctrl_status_t st;
static void run(ctrl_input_t *in, uint32_t ms)
{
    for (uint32_t t = 0; t < ms; t += 200) { now += 200; Ctrl_Update(in, now); }
    Ctrl_GetStatus(&st);
}

int main(void)
{
    Settings_Default();
    ctrl_input_t in = { true, 30.0f, true, 60.0f, true, 15.0f };
    now = 1000;
    Ctrl_Init(now);

    run(&in, 1000);
    EXPECT(st.state == CTRL_IDLE && !st.comp && !st.fan_evap, "IDLE: tat ca OFF");

    Ctrl_Start(); run(&in, 400);
    EXPECT(st.state == CTRL_STARTING && st.fan_evap && !st.comp, "STARTING: quat lanh chay truoc");

    run(&in, 10000);
    EXPECT(st.state == CTRL_RUNNING, "RUNNING sau start_delay");
    EXPECT(!st.comp && st.comp_wait_s > 0, "May nen cho min_off sau cap dien");

    run(&in, 180000);
    EXPECT(st.comp && st.fan_cond, "May nen + quat nong chay (mode 0)");

    in.temp = 56.0f; in.hum = 14.0f; run(&in, 61000);
    EXPECT(!st.comp && st.target_reached, "Dat nhiet+am -> tat may nen");

    in.hum = 16.0f; run(&in, 1000);
    EXPECT(!st.comp, "Trong vung tre am -> van tat");
    in.hum = 19.0f; run(&in, 1000);
    EXPECT(!st.comp && st.comp_demand, "Can chay nhung dang cho min_off");
    run(&in, 180000);
    EXPECT(st.comp, "Het min_off -> may nen chay lai");

    in.press = 31.0f; run(&in, 200);
    EXPECT(st.state == CTRL_FAULT && !st.comp && (st.faults & FAULT_PRESS_HIGH), "Ap cao -> FAULT, tat may nen ngay");
    EXPECT(st.fan_cond, "FAULT ap cao: quat nong van chay");
    in.press = 15.0f; run(&in, 61000);
    EXPECT(!st.fan_evap && !st.fan_cond, "FAULT: quat tat sau fan_post");
    Ctrl_ResetFault(); run(&in, 400);
    EXPECT(st.state == CTRL_IDLE && st.faults == 0, "Reset loi -> IDLE");

    in.temp_ok = false; run(&in, 2000);
    EXPECT(st.state == CTRL_FAULT && (st.faults & FAULT_TEMP_SENSOR), "Mat cam bien nhiet -> FAULT");
    in.temp_ok = true; Ctrl_ResetFault(); run(&in, 400);

    in.hum_ok = false; in.temp = 40.0f; Ctrl_Start(); run(&in, 200000);
    EXPECT(st.state == CTRL_RUNNING && (st.warnings & WARN_HUM_SENSOR) && st.comp, "Mat CB am: chay theo nhiet + canh bao");

    Ctrl_Stop(); run(&in, 400);
    EXPECT(st.state == CTRL_STOPPING && !st.comp && st.fan_evap, "Stop: tat may nen, quat chay them");
    run(&in, 61000);
    EXPECT(st.state == CTRL_IDLE && !st.fan_evap, "Het fan_post -> IDLE");

    printf("\n%s (%d loi)\n", fails ? "CO LOI" : "TAT CA PASS", fails);
    return fails;
}
