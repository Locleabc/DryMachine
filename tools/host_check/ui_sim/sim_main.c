/* Mô phỏng giao diện trên PC: chạy nguyên các file App/ui với ILI9341 giả lập,
 * bấm phím theo kịch bản, kiểm tra lệnh gửi ra và lưu ảnh từng màn hình (PPM).
 * Build: xem tools/host_check/check.sh */
#include "ui.h"
#include <stdio.h>
#include <string.h>

void Sim_Save(const char *path);
int  Sim_Overflow(void);

static const char *names[7] = { "Rau thơm", "Rau củ", "Trái cây", "Nấm", "Thịt – Cá", "Hạt – Nông sản", "Tự do" };
static const float dt[7] = { 40, 50, 55, 50, 60, 45, 50 }, dh[7] = { 20, 15, 15, 18, 12, 12, 15 };

static ui_view_t v;
static ui_cmd_t  last_cmd;
static int       cmd_count, fails;
static uint32_t  now = 1000;
static const char *outdir = ".";

/* ---- menu kỹ thuật giả ---- */
static float tech_val[3] = { 2.0f, 30.0f, 180.0f };
static const ui_param_desc_t tech_desc[3] = {
    { "Trễ nhiệt", "°C", 0.5f, 10, 0.5f, 1, NULL }, { "Dàn lạnh: chờ", "s", 0, 600, 10, 0, NULL },
    { "Máy nén chạy min", "s", 10, 600, 10, 0, NULL } };
static uint8_t t_count(void) { return 3; }
static bool t_desc(uint8_t i, ui_param_desc_t *o) { if (i >= 3) return false; *o = tech_desc[i]; return true; }
static float t_get(uint8_t i) { return tech_val[i]; }
static void t_set(uint8_t i, float x) { tech_val[i] = x; }
static const ui_param_if_t tech = { t_count, t_desc, t_get, t_set };

/* ---- danh sách cài đặt chu trình giả (giống settings nhóm PROCESS) ---- */
static float proc_val[4] = { 12, 60, 60, 75 };
static const ui_param_desc_t proc_desc[4] = {
    { "GĐ3: độ ẩm đạt", "%", 3, 80, 1, 0, NULL }, { "GĐ3: giữ ẩm", "phút", 0, 999, 5, 0, NULL },
    { "Máy nén chờ bật lại", "s", 10, 600, 5, 0, NULL }, { "Nhiệt độ bảo vệ", "°C", 50, 95, 1, 0, NULL } };
static uint8_t p_count(void) { return 4; }
static bool p_desc(uint8_t i, ui_param_desc_t *o) { if (i >= 4) return false; *o = proc_desc[i]; return true; }
static float p_get(uint8_t i) { return proc_val[i]; }
static void p_set(uint8_t i, float x) { proc_val[i] = x; if (i == 0) v.hum_gd3 = x; if (i == 1) v.gd3_min = (uint16_t)x; if (i == 2) v.comp_restart_s = (uint16_t)x; }
static const ui_param_if_t proc = { p_count, p_desc, p_get, p_set };

/* ---- danh sách giả lập giả (giống app.c) ---- */
static const char *const sim_mode_ch[] = { "Tắt", "Mô hình", "Chỉnh tay" };
static const char *const sim_speed_ch[] = { "x1", "x10", "x60", "x300" };
static float sim_val[3] = { 0, 0, 30 };
static const ui_param_desc_t sim_d[3] = {
    { "Giả lập", "", 0, 2, 1, 0, sim_mode_ch }, { "Tua nhanh", "", 0, 3, 1, 0, sim_speed_ch },
    { "Nhiệt độ", "°C", 0, 100, 0.5f, 1, NULL } };
static uint8_t s_count(void) { return 3; }
static bool s_desc(uint8_t i, ui_param_desc_t *o) { if (i >= 3) return false; *o = sim_d[i]; return true; }
static float s_get(uint8_t i) { return sim_val[i]; }
static void s_set(uint8_t i, float x) { sim_val[i] = x; if (i == 0) v.sim_text = (x > 0.5f) ? "GIẢ LẬP · Mô hình x60" : NULL; }
static const ui_param_if_t simif = { s_count, s_desc, s_get, s_set };

static bool on_cmd(const ui_cmd_t *c)
{
    last_cmd = *c; cmd_count++;
    switch (c->type) {
    case UI_CMD_SELECT_PRESET: v.preset = c->u.preset.idx; v.temp_set = v.preset_temp[v.preset]; v.hum_set = v.preset_hum[v.preset]; break;
    case UI_CMD_SET_PRESET:
        v.preset_temp[c->u.preset.idx] = c->u.preset.temp; v.preset_hum[c->u.preset.idx] = c->u.preset.hum;
        if (c->u.preset.select) { v.preset = c->u.preset.idx; v.temp_set = c->u.preset.temp; v.hum_set = c->u.preset.hum; }
        break;
    case UI_CMD_START_STOP: v.state = (v.state == UI_ST_IDLE) ? UI_ST_RUNNING : UI_ST_IDLE; break;
    case UI_CMD_RESET_FAULT: v.fault_text = NULL; break;
    case UI_CMD_OUT_TEST: if (c->u.test.op != UI_TEST_TOGGLE) v.out_test = (c->u.test.op == UI_TEST_BEGIN); break;
    default: break;
    }
    return true;
}

#define EXPECT(c, m) do { if (!(c)) { printf("FAIL: %s\n", m); fails++; } else printf("ok  : %s\n", m); } while (0)

static void tick(int n) { for (int i = 0; i < n; i++) { now += 100; UI_Update(&v, now); } }
static void key(ui_key_t k, ui_press_t p) { UI_Key(k, p); tick(1); }
static void shot(const char *name)
{
    char p[256];
    tick(3);
    snprintf(p, sizeof(p), "%s/%s.ppm", outdir, name);
    Sim_Save(p);
    int o = Sim_Overflow();
    if (o) { printf("FAIL: %s - %d ky tu ve ra ngoai man hinh\n", name, o); fails++; }
}

int main(int argc, char **argv)
{
    if (argc > 1) outdir = argv[1];
    memset(&v, 0, sizeof(v));
    for (int i = 0; i < 7; i++) { v.preset_temp[i] = dt[i]; v.preset_hum[i] = dh[i]; }
    v.preset = 2; v.temp_set = 55; v.hum_set = 15;
    v.temp_ok = true; v.temp = 54.6f; v.hum_ok = true; v.hum = 21.4f; v.evap_ok[0] = v.evap_ok[1] = true; v.evap[0] = 3.2f; v.evap[1] = 2.8f;
    v.state = UI_ST_RUNNING; v.comp = true; v.fan_level = 3; v.fan_evap = true;
    v.phase_text = "GĐ1 – gia nhiệt"; v.stage = 0;
    v.stage_fan[0] = 4; v.stage_fan[1] = 3; v.stage_fan[2] = 2; v.stage_fan[3] = 1; v.stage_fan[4] = 5;
    v.gd3_min = 60; v.gd4_min = 30; v.hum_gd3 = 12; v.hum_gd4 = 10; v.end_temp = 40; v.temp_max = 75; v.comp_restart_s = 60;
    v.run_s = 5025;
    v.hum_temp_ok = true; v.hum_temp = 28.3f;
    v.hist_count = 3;
    v.hist[0].text = "Quá nhiệt"; v.hist[1].text = "Mất cảm biến nhiệt"; v.hist[2].text = "Quá nhiệt";

    static const char *const onames[7] = { "Máy nén", "Quạt dàn lạnh", "Quạt nóng cấp 1", "Quạt nóng cấp 2",
                                           "Quạt nóng cấp 3", "Quạt nóng cấp 4", "Quạt nóng cấp 5" };
    static const char *const opins[7] = { "IN1 · PB4", "IN2 · PB5", "IN3 · PB6", "IN4 · PB7",
                                          "IN5 · PA1", "IN6 · PA2", "IN7 · PA3" };
    ui_config_t cfg = { names, 7, &tech, &proc, &simif, on_cmd, onames, opins, 7 };
    UI_Init(&cfg);
    tick(2);

    shot("01_main");
    key(UI_KEY_DOWN, UI_PRESS_SHORT);  shot("02_run");
    key(UI_KEY_ENTER, UI_PRESS_SHORT);
    EXPECT(last_cmd.type == UI_CMD_START_STOP, "Trang 2: ENTER gui lenh chay/dung");
    key(UI_KEY_ENTER, UI_PRESS_SHORT);                          /* chạy lại */
    key(UI_KEY_EXIT, UI_PRESS_LONG);                            /* mở GIẢ LẬP */
    key(UI_KEY_ENTER, UI_PRESS_SHORT); key(UI_KEY_UP, UI_PRESS_SHORT); key(UI_KEY_ENTER, UI_PRESS_SHORT);
    sim_val[1] = 2; tick(3); shot("12_sim_list");
    int before = cmd_count;
    key(UI_KEY_EXIT, UI_PRESS_SHORT);
    EXPECT(v.sim_text != NULL && cmd_count == before, "Gia lap: bat Mo hinh, thoat KHONG luu Flash");
    tick(3); shot("12b_run_sim");
    key(UI_KEY_DOWN, UI_PRESS_SHORT);  shot("03_cycle");
    key(UI_KEY_ENTER, UI_PRESS_SHORT); shot("03b_cycle_settings");
    key(UI_KEY_ENTER, UI_PRESS_SHORT); key(UI_KEY_UP, UI_PRESS_SHORT); key(UI_KEY_ENTER, UI_PRESS_SHORT);
    EXPECT(proc_val[0] == 13 && v.hum_gd3 == 13, "Trang 3: sua GD3 do am 12 -> 13%");
    key(UI_KEY_EXIT, UI_PRESS_SHORT);
    EXPECT(last_cmd.type == UI_CMD_SAVE_SETTINGS, "Thoat danh sach -> luu cai dat");
    v.phase_text = "GĐ3 – giữ ẩm"; v.stage = 2; v.phase_left_s = 2321; v.hold_counting = true; v.fan_level = 2;
    tick(6); shot("03c_cycle_gd3");
    key(UI_KEY_DOWN, UI_PRESS_SHORT);
    v.fault_text = "Quá nhiệt"; v.state = UI_ST_FAULT; v.comp = false;
    tick(6);
    shot("04_faults");
    key(UI_KEY_ENTER, UI_PRESS_SHORT);
    EXPECT(last_cmd.type == UI_CMD_RESET_FAULT, "Trang 4: ENTER xoa loi");
    v.state = UI_ST_RUNNING; v.comp = true;
    key(UI_KEY_DOWN, UI_PRESS_SHORT);                           /* trang 5: đầu ra */
    v.out_cmd = 0x0B; v.out_relay = 0x0B;                       /* máy nén, quạt lạnh, cấp 2 */
    tick(3); shot("05_outputs");
    v.out_relay = 0;  tick(3); shot("05b_outputs_sim");         /* giả lập, relay không đóng */
    v.out_cmd = v.out_relay = 0;
    key(UI_KEY_ENTER, UI_PRESS_SHORT);                          /* vào test đầu ra */
    EXPECT(last_cmd.type == UI_CMD_OUT_TEST && last_cmd.u.test.op == UI_TEST_BEGIN, "Trang 5: ENTER vao test dau ra");
    v.out_test = true;
    key(UI_KEY_DOWN, UI_PRESS_SHORT);                           /* chọn Quạt dàn lạnh (không chuyển trang) */
    key(UI_KEY_ENTER, UI_PRESS_SHORT);
    EXPECT(last_cmd.type == UI_CMD_OUT_TEST && last_cmd.u.test.op == UI_TEST_TOGGLE && last_cmd.u.test.idx == 1,
           "Test: DOWN chon dong 2, ENTER bat/tat quat dan lanh");
    v.out_cmd = v.out_relay = 0x02;
    key(UI_KEY_DOWN, UI_PRESS_SHORT); key(UI_KEY_DOWN, UI_PRESS_SHORT);   /* Quạt nóng cấp 2 */
    key(UI_KEY_ENTER, UI_PRESS_SHORT);
    EXPECT(last_cmd.u.test.idx == 3, "Test: chon quat nong cap 2");
    v.out_cmd = v.out_relay = 0x0A;
    tick(3); shot("05c_outputs_test");
    key(UI_KEY_ENTER, UI_PRESS_LONG);                           /* không mở menu chế độ khi test */
    key(UI_KEY_EXIT, UI_PRESS_SHORT);
    EXPECT(last_cmd.type == UI_CMD_OUT_TEST && last_cmd.u.test.op == UI_TEST_END, "Test: EXIT thoat, tat het");
    v.out_test = false; v.out_cmd = v.out_relay = 0;
    key(UI_KEY_DOWN, UI_PRESS_SHORT);                           /* vòng về trang 1 */

    v.hold_counting = false; tick(3); shot("01b_main_gd3_wait");
    key(UI_KEY_DOWN, UI_PRESS_SHORT); tick(3); shot("02b_run_gd3_wait");
    key(UI_KEY_UP, UI_PRESS_SHORT);
    key(UI_KEY_ENTER, UI_PRESS_LONG);  shot("06_preset_menu");
    key(UI_KEY_UP, UI_PRESS_SHORT);                             /* Rau cu */
    key(UI_KEY_ENTER, UI_PRESS_SHORT);
    EXPECT(last_cmd.type == UI_CMD_SELECT_PRESET && last_cmd.u.preset.idx == 1 && v.preset == 1, "Chon che do dat san 'Rau cu'");
    shot("06b_main_after_select");

    key(UI_KEY_ENTER, UI_PRESS_LONG);
    for (int i = 0; i < 5; i++) key(UI_KEY_DOWN, UI_PRESS_SHORT); /* 1 -> 6 = Tu do */
    key(UI_KEY_ENTER, UI_PRESS_SHORT); shot("07_edit_custom");  /* "50C 15%" */
    key(UI_KEY_UP, UI_PRESS_SHORT);    key(UI_KEY_ENTER, UI_PRESS_SHORT);  /* 6 */
    key(UI_KEY_UP, UI_PRESS_SHORT);    key(UI_KEY_UP, UI_PRESS_SHORT); key(UI_KEY_ENTER, UI_PRESS_SHORT); /* 2 */
    key(UI_KEY_UP, UI_PRESS_SHORT);    key(UI_KEY_ENTER, UI_PRESS_SHORT);  /* 2 */
    key(UI_KEY_DOWN, UI_PRESS_SHORT);  shot("07b_edit_custom_last");
    key(UI_KEY_ENTER, UI_PRESS_SHORT);                                     /* 4 -> lưu */
    EXPECT(last_cmd.type == UI_CMD_SET_PRESET && last_cmd.u.preset.idx == 6 && last_cmd.u.preset.temp == 62 &&
           last_cmd.u.preset.hum == 24 && last_cmd.u.preset.select, "Tu do nhap tung chu so -> 62C 24%, ap dung ngay");
    shot("08_main_custom");

    key(UI_KEY_EXIT, UI_PRESS_LONG);   shot("10_tech");
    key(UI_KEY_ENTER, UI_PRESS_SHORT); key(UI_KEY_UP, UI_PRESS_SHORT); key(UI_KEY_ENTER, UI_PRESS_SHORT);
    cmd_count = 0;
    key(UI_KEY_EXIT, UI_PRESS_SHORT);
    EXPECT(tech_val[0] == 2.5f && last_cmd.type == UI_CMD_SAVE_SETTINGS, "Menu ky thuat: sua + luu");

    key(UI_KEY_ENTER, UI_PRESS_LONG); key(UI_KEY_UP, UI_PRESS_SHORT);   /* Tu do -> Hat - Nong */
    key(UI_KEY_ENTER, UI_PRESS_LONG); shot("11_edit_fixed_preset");
    key(UI_KEY_EXIT, UI_PRESS_SHORT);
    EXPECT(v.preset == 6, "EXIT khi dang nhap = huy, khong doi che do");

    printf("\n%s (%d loi)\n", fails ? "UI CO LOI" : "UI PASS", fails);
    return fails;
}
