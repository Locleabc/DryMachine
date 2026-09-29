/* Chạy trọn 1 chu trình sấy trên PC: bộ điều khiển thật (control/dryer_ctrl.c) + mô hình buồng sấy
 * (services/sim_plant.c) – cùng mô hình dùng trong chế độ GIẢ LẬP trên máy. In ra mốc chuyển giai đoạn.
 * Chạy: sh tools/host_check/check.sh   (bước 5)  hoặc tự build:
 *   gcc -std=c11 -IApp/control -IApp/services tools/host_check/sim_cycle.c \
 *       App/control/dryer_ctrl.c App/services/sim_plant.c -o sim_cycle && ./sim_cycle [thucong]
 */
#include "dryer_ctrl.h"
#include "sim_plant.h"
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    dryer_params_t p;
    DryerCtrl_DefaultParams(&p);
    bool overtemp = (argc > 1 && strcmp(argv[1], "quanhiet") == 0);
    p.mode = (argc > 1 && strcmp(argv[1], "thucong") == 0) ? 1.0f : 0.0f;
    p.dry_time_h = 4.0f;
    if (overtemp) p.temp_max = 50.0f;          /* đặt bảo vệ thấp hơn nhiệt độ sấy → chắc chắn quá nhiệt */
    bool had_fault = false;

    dryer_ctrl_t c; dryer_output_t o; dryer_status_t st; sim_plant_t s;
    SimPlant_Init(&s, 30.0f, 65.0f);
    uint32_t now = 0;
    DryerCtrl_Init(&c, now);
    DryerCtrl_Command(&c, DRYER_CMD_START);

    printf("Che do %s: dat %.0f C / %.0f %%, bao ve %.0f C, dung GD5 %.0f C%s\n",
           p.mode > 0.5f ? "THU CONG" : "TU DONG", p.temp_set, p.hum_set, p.temp_max, p.end_temp,
           p.mode > 0.5f ? "" : ", thoi gian say 4 h");
    int last_ph = -1, last_st = -1;
    float tmax = 0;
    uint32_t comp_starts = 0; bool prev_comp = false;
    for (uint32_t t = 0; t < 12 * 3600; t++) {
        now += 1000;
        dryer_input_t in = { true, s.temp, true, s.hum, true, s.press };
        DryerCtrl_Step(&c, &p, &in, now, &o);
        DryerCtrl_GetStatus(&c, &st);
        SimPlant_Step(&s, 1.0f, o.comp, o.fan_level, o.fan_evap);
        if (s.temp > tmax) tmax = s.temp;
        if (o.comp && !prev_comp) comp_starts++;
        prev_comp = o.comp;
        if ((int)st.phase != last_ph || (int)st.state != last_st) {
            printf("  %6.1f phut  %-10s %-24s T=%5.1f H=%5.1f P=%4.1f quat=%d MN=%d\n", t / 60.0,
                   DryerCtrl_StateName(st.state), DryerCtrl_PhaseName(st.phase), s.temp, s.hum, s.press,
                   o.fan_level, o.comp);
            last_ph = st.phase; last_st = st.state;
        }
        if (st.state == DRYER_IDLE && t > 10) break;
        if (st.state == DRYER_FAULT && !had_fault) {
            printf("  LOI: %s\n", DryerCtrl_FaultText(st.faults));
            had_fault = true;
            if (!DryerCtrl_OvertempCooling(&c)) break;
        }
    }
    if (overtemp) {
        bool ok = had_fault && st.state == DRYER_IDLE && s.temp <= p.temp_recover;
        printf("  Qua nhiet %.1f C -> lam mat toi %.1f C -> %s\n", tmax, s.temp, ok ? "HET LOI, DUNG MAY" : "SAI");
        return ok ? 0 : 1;
    }
    printf("  Nhiet do cao nhat %.1f C, may nen khoi dong %lu lan, %s\n", tmax, (unsigned long)comp_starts,
           st.finished ? "HOAN THANH" : "chua xong");
    return st.finished ? 0 : 1;
}
