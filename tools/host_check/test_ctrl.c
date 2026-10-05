/* Test logic control/ trên PC – KHÔNG cần HAL (tầng control độc lập phần cứng).
 * Chạy: gcc -std=c11 -IApp/control tools/host_check/test_ctrl.c App/control/dryer_ctrl.c App/control/fan_speed.c -o t && ./t
 */
#include "dryer_ctrl.h"
#include "fan_speed.h"
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

/* mô phỏng buồng sấy đơn giản: máy nén chạy → nóng lên, khô đi; tắt → nguội */
static void run_plant(dryer_input_t *in, uint32_t ms)
{
    for (uint32_t t = 0; t < ms; t += 1000) {
        run(in, 1000);
        if (out.comp) { in->temp += 0.05f; in->hum -= 0.02f; }
        else          { in->temp -= 0.03f; }
        if (in->hum < 5) in->hum = 5;
    }
}

/* chưa gắn cảm biến: khởi động báo lỗi, bật giả lập (xoá lỗi + giá trị hợp lệ) → chạy được */
static void test_no_sensor_then_sim(void)
{
    printf("\n--- CHUA CO CAM BIEN -> GIA LAP ---\n");
    DryerCtrl_DefaultParams(&p);
    p.press_enable = 0.0f;                                  /* chưa gắn cảm biến áp suất */
    dryer_input_t in = { false, 0.0f, false, 0.0f, false, 0.0f };
    now = 1000;
    DryerCtrl_Init(&c, now);
    run(&in, 3000);
    EXPECT(st.state == DRYER_FAULT, "Khong co cam bien nhiet -> loi, khoa may");
    in = (dryer_input_t){ true, 30.0f, true, 65.0f, false, 0.0f };   /* giá trị nhập tay */
    DryerCtrl_Command(&c, DRYER_CMD_RESET_FAULT);
    run(&in, 1000);
    EXPECT(st.state == DRYER_IDLE, "Bat gia lap: xoa loi -> san sang");
    DryerCtrl_Command(&c, DRYER_CMD_TOGGLE);
    run(&in, 1000);
    EXPECT(st.state == DRYER_STARTING, "Chay duoc khi khong co cam bien ap suat (bao ve ap = Tat)");
}

static void test_auto(void)
{
    printf("\n--- TU DONG ---\n");
    DryerCtrl_DefaultParams(&p);
    dryer_input_t in = { true, 30.0f, true, 60.0f, true, 15.0f };
    now = 1000;
    DryerCtrl_Init(&c, now);
    run(&in, 1000);
    EXPECT(st.state == DRYER_IDLE && !out.comp && out.fan_level == 0, "IDLE: tat ca OFF");

    DryerCtrl_Command(&c, DRYER_CMD_TOGGLE); run(&in, 1000);
    EXPECT(st.state == DRYER_STARTING && out.fan_evap && out.fan_level == 3 && !out.comp, "Khoi dong: quat nong cap 3, may nen chua chay");
    run(&in, 60000);
    EXPECT(st.state == DRYER_RUNNING && st.phase == PH_AUTO_DRY && out.comp, "Sau 60 s: hut am, may nen chay");

    in.temp = 70.0f; run(&in, 5000);
    EXPECT(out.comp && st.phase == PH_AUTO_DRY, "Hut am: vuot nhiet dat van chay (chi dung khi dat am)");

    in.hum = 14.0f; run(&in, 31000);
    EXPECT(st.phase == PH_AUTO_HOLD && !out.comp, "Dat am -> may nen dung, sang giu nhiet");

    in.temp = 50.0f; in.hum = 40.0f; run(&in, 10000);
    EXPECT(!out.comp && st.comp_wait_s > 0, "Giu nhiet: can chay nhung cho bat lai (60 s)");
    run(&in, 60000);
    EXPECT(out.comp && st.phase == PH_AUTO_HOLD, "Het thoi gian cho -> may nen chay, bo qua do am");

    in.temp = 56.0f; run(&in, 31000);
    EXPECT(!out.comp, "Dat nhiet -> may nen tat");

    p.dry_time_h = 0.05f; run(&in, 1000);   /* đã chạy > 3 phút */
    EXPECT(st.phase == PH_AUTO_COOL && !out.comp && out.fan_level == 3 && out.fan_evap, "Het gio say -> lam mat");
    in.temp = 39.0f; run(&in, 1000);
    EXPECT(st.state == DRYER_IDLE && st.finished && out.fan_level == 0 && !out.fan_evap, "Nguoi toi 40 C -> ket thuc");
}

static void test_manual(void)
{
    printf("\n--- THU CONG ---\n");
    DryerCtrl_DefaultParams(&p);
    p.mode = 1; p.gd3_min = 2; p.gd4_min = 1; p.dry_time_h = 0.01f;   /* thời gian sấy phải bị bỏ qua */
    p.stage_fan[0] = 4; p.stage_fan[1] = 3; p.stage_fan[2] = 2; p.stage_fan[3] = 1; p.stage_fan[4] = 5;
    dryer_input_t in = { true, 30.0f, true, 60.0f, true, 15.0f };
    now = 1000;
    DryerCtrl_Init(&c, now);
    DryerCtrl_Command(&c, DRYER_CMD_START); run(&in, 1000);
    EXPECT(st.state == DRYER_STARTING && out.fan_level == 4 && !out.comp, "GD1: quat cap GD1, cho 60 s");
    run(&in, 60000);
    EXPECT(st.phase == PH_GD1 && out.comp, "GD1: may nen ON sau 60 s");

    run_plant(&in, 600000);
    EXPECT(st.phase == PH_GD2 || st.phase == PH_GD3, "Dat nhiet -> GD2");
    EXPECT(st.phase != PH_AUTO_COOL && st.state == DRYER_RUNNING, "Thu cong: bo qua thoi gian say trang 3");

    in.hum = 20.0f; in.temp = 55.0f; run(&in, 1000);
    if (st.phase == PH_GD2) {
        EXPECT(out.fan_level == 3, "GD2: quat cap GD2");
        in.hum = 14.0f; run(&in, 1000);
    }
    EXPECT(st.phase == PH_GD3 && out.fan_level == 2, "Dat am -> GD3, quat cap GD3");
    EXPECT(st.phase_left_s > 100 && st.phase_left_s <= 120, "GD3 dem nguoc 2 phut");
    in.temp = 50.0f; run(&in, 90000);
    EXPECT(out.comp, "GD3: giu nhiet, may nen chay khi nguoi");
    run(&in, 30000);
    EXPECT(st.phase == PH_GD4 && out.fan_level == 1, "Het GD3 -> GD4, quat cap GD4");
    run(&in, 61000);
    EXPECT(st.phase == PH_GD5 && !out.comp && out.fan_level == 5, "Het GD4 -> GD5: may nen OFF, quat cap GD5");
    in.temp = 45.0f; run(&in, 5000);
    EXPECT(st.phase == PH_GD5 && st.state == DRYER_RUNNING, "GD5: chua nguoi toi nguong -> van chay quat");
    in.temp = 40.0f; run(&in, 1000);
    EXPECT(st.state == DRYER_IDLE && st.finished, "GD5: nguoi toi nguong -> ket thuc chu trinh");
}

static void test_protect(void)
{
    printf("\n--- BAO VE ---\n");
    DryerCtrl_DefaultParams(&p);
    dryer_input_t in = { true, 40.0f, true, 60.0f, true, 15.0f };
    now = 1000;
    DryerCtrl_Init(&c, now);
    DryerCtrl_Command(&c, DRYER_CMD_START); run(&in, 62000);
    EXPECT(out.comp, "Dang chay");
    p.temp_max = 70.0f; in.temp = 71.0f; run(&in, 200);
    EXPECT(st.state == DRYER_FAULT && !out.comp && (st.faults & DRYER_FAULT_OVERTEMP), "Qua nhiet bao ve (chinh duoc) -> dung may");
    EXPECT(out.fan_level == 5 && out.fan_evap, "Qua nhiet: quat dan lanh + quat nong cap 5");
    in.temp = 50.0f; run(&in, 300000);
    EXPECT(st.state == DRYER_FAULT && !out.comp && out.fan_level == 5 && out.fan_evap, "Qua nhiet: quat chay lien tuc (5 phut, 50 C)");
    DryerCtrl_Command(&c, DRYER_CMD_RESET_FAULT); run(&in, 400);
    EXPECT(st.state == DRYER_FAULT && (st.faults & DRYER_FAULT_OVERTEMP), "Qua nhiet: chua nguoi 30 C -> khong reset duoc");
    in.temp = 30.5f; run(&in, 1000);
    EXPECT(st.state == DRYER_FAULT && out.fan_level == 5, "Qua nhiet: 30.5 C van lam mat");
    in.temp = 30.0f; run(&in, 400);
    EXPECT(st.state == DRYER_IDLE && st.faults == 0 && out.fan_level == 0 && !out.fan_evap && !out.comp,
           "Qua nhiet: nguoi toi 30 C -> het loi, tat het");
    p.temp_recover = 80.0f;
    EXPECT(DryerCtrl_RecoverTemp(&p) == 65.0f, "Nhiet do het qua nhiet luon <= bao ve - 5");
    p.temp_recover = 30.0f;
    in.temp = 40.0f;

    DryerCtrl_Command(&c, DRYER_CMD_START); run(&in, 62000);
    in.press = 31.0f; run(&in, 200);
    EXPECT(st.state == DRYER_FAULT && (st.faults & DRYER_FAULT_PRESS_HIGH) && out.fan_level == 5, "Ap cao -> loi, quat nong cap 5");
    in.press = 15.0f; DryerCtrl_Command(&c, DRYER_CMD_RESET_FAULT); run(&in, 61000);

    DryerCtrl_Command(&c, DRYER_CMD_START); run(&in, 62000);
    DryerCtrl_Command(&c, DRYER_CMD_TOGGLE); run(&in, 400);
    EXPECT(st.state == DRYER_STOPPING && !out.comp && out.fan_evap, "Dung tay -> tat may nen, quat chay them");
    run(&in, 61000);
    EXPECT(st.state == DRYER_IDLE && !st.finished, "Het fan_post -> IDLE (khong tinh la hoan thanh)");
}

static void test_fan_speed(void)
{
    printf("\n--- QUAT 5 CAP ---\n");
    fan_speed_t f;
    FanSpeed_Init(&f, 1000);
    EXPECT(FanSpeed_Step(&f, 3, 1000) == 1, "Bat lan dau: dong ngay relay 1");
    EXPECT(FanSpeed_Step(&f, 3, 1500) == 1, "Chua du 1 s: van 1 relay");
    EXPECT(FanSpeed_Step(&f, 3, 2000) == 2, "Du 1 s: them relay 2");
    EXPECT(FanSpeed_Step(&f, 3, 3000) == 3, "Them relay 3 -> cap 3");
    EXPECT(FanSpeed_Step(&f, 3, 9000) == 3, "Giu cap 3");
    EXPECT(FanSpeed_RelayOn(3, 1) && FanSpeed_RelayOn(3, 3) && !FanSpeed_RelayOn(3, 4), "Cap 3 = relay 1,2,3");
    EXPECT(FanSpeed_Step(&f, 5, 9100) == 4 && FanSpeed_Step(&f, 5, 9500) == 4, "Len cap 5: them relay 4, cho");
    EXPECT(FanSpeed_Step(&f, 5, 10100) == 5, "Them relay 5 -> cap 5");
    EXPECT(FanSpeed_Step(&f, 2, 10200) == 2, "Giam cap: nha ngay relay 3..5");
    EXPECT(FanSpeed_Step(&f, 0, 10300) == 0, "Tat quat: tat ngay");
}

int main(void)
{
    test_no_sensor_then_sim();
    test_auto();
    test_manual();
    test_protect();
    test_fan_speed();
    printf("\n%s (%d loi)\n", fails ? "CO LOI" : "TAT CA PASS", fails);
    return fails;
}
