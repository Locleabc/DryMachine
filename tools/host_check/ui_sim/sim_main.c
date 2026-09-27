/* Mô phỏng giao diện trên PC: chạy nguyên các file App/ui với ILI9341 giả lập,
 * bấm phím theo kịch bản, kiểm tra lệnh gửi ra và lưu ảnh từng màn hình (PPM).
 * Build: xem tools/host_check/check.sh */
#include "ui.h"
#include <stdio.h>
#include <string.h>

void Sim_Save(const char *path);
int  Sim_Overflow(void);

static const char *names[7] = { "Rau thom", "Rau cu", "Trai cay", "Nam", "Thit - Ca", "Hat - Nong", "Tu do" };
static const float dt[7] = { 40, 50, 55, 50, 60, 45, 50 }, dh[7] = { 20, 15, 15, 18, 12, 12, 15 };

static ui_view_t v;
static ui_cmd_t  last_cmd;
static int       cmd_count, fails;
static uint32_t  now = 1000;
static const char *outdir = ".";

/* ---- menu kỹ thuật giả ---- */
static float tech_val[3] = { 2.0f, 30.0f, 180.0f };
static const ui_param_desc_t tech_desc[3] = {
    { "Tre nhiet", "C", 0.5f, 10, 0.5f, 1 }, { "Ap cao ngat", "bar", 5, 45, 0.5f, 1 }, { "MN nghi min", "s", 30, 600, 10, 0 } };
static uint8_t t_count(void) { return 3; }
static bool t_desc(uint8_t i, ui_param_desc_t *o) { if (i >= 3) return false; *o = tech_desc[i]; return true; }
static float t_get(uint8_t i) { return tech_val[i]; }
static void t_set(uint8_t i, float x) { tech_val[i] = x; }
static const ui_param_if_t tech = { t_count, t_desc, t_get, t_set };

static bool on_cmd(const ui_cmd_t *c)
{
    last_cmd = *c; cmd_count++;
    switch (c->type) {
    case UI_CMD_SELECT_PRESET: v.preset = c->u.preset.idx; v.temp_set = v.preset_temp[v.preset]; v.hum_set = v.preset_hum[v.preset]; break;
    case UI_CMD_SET_PRESET:
        v.preset_temp[c->u.preset.idx] = c->u.preset.temp; v.preset_hum[c->u.preset.idx] = c->u.preset.hum;
        if (c->u.preset.select) { v.preset = c->u.preset.idx; v.temp_set = c->u.preset.temp; v.hum_set = c->u.preset.hum; }
        break;
    case UI_CMD_SET_DRY_TIME: v.dry_time_min = c->u.minutes; break;
    case UI_CMD_SET_CLOCK: v.day = c->u.clock.day; v.mon = c->u.clock.mon; v.year = c->u.clock.year; v.hour = c->u.clock.hour; v.min = c->u.clock.min; break;
    case UI_CMD_START_STOP: v.state = (v.state == UI_ST_IDLE) ? UI_ST_RUNNING : UI_ST_IDLE; break;
    case UI_CMD_RESET_FAULT: v.fault_text = NULL; break;
    default: break;
    }
    return true;
}

#define EXPECT(c, m) do { if (!(c)) { printf("FAIL: %s\n", m); fails++; } else printf("ok  : %s\n", m); } while (0)

static void tick(int n) { for (int i = 0; i < n; i++) { now += 100; v.sec = (uint8_t)((v.sec + (i % 10 == 9)) % 60); UI_Update(&v, now); } }
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
    v.temp_ok = true; v.temp = 54.6f; v.hum_ok = true; v.hum = 21.4f; v.press_ok = true; v.press = 18.3f;
    v.state = UI_ST_RUNNING; v.comp = true; v.fan_cond = true; v.fan_evap = true;
    v.run_s = 5025; v.dry_time_min = 360;
    v.clock_ok = true; v.year = 2026; v.mon = 9; v.day = 27; v.hour = 14; v.min = 5; v.sec = 32;
    v.hist_count = 3;
    strcpy(v.hist[0].when, "27/09 09:12"); v.hist[0].text = "AP SUAT CAO";
    strcpy(v.hist[1].when, "25/09 21:40"); v.hist[1].text = "MAT CB NHIET";
    strcpy(v.hist[2].when, "20/09 06:03"); v.hist[2].text = "QUA NHIET";

    ui_config_t cfg = { names, 7, &tech, on_cmd };
    UI_Init(&cfg);
    tick(2);

    shot("01_main");
    key(UI_KEY_DOWN, UI_PRESS_SHORT);  shot("02_run");
    key(UI_KEY_ENTER, UI_PRESS_SHORT);
    EXPECT(last_cmd.type == UI_CMD_START_STOP, "Trang 2: ENTER gui lenh chay/dung");
    key(UI_KEY_ENTER, UI_PRESS_SHORT);                          /* chạy lại */
    key(UI_KEY_DOWN, UI_PRESS_SHORT);  shot("03_timer");
    key(UI_KEY_ENTER, UI_PRESS_SHORT); shot("03b_timer_edit");  /* "06:00" */
    key(UI_KEY_UP, UI_PRESS_SHORT);                             /* 1 */
    key(UI_KEY_ENTER, UI_PRESS_SHORT);
    key(UI_KEY_DOWN, UI_PRESS_SHORT);                           /* 6 -> 5 */
    key(UI_KEY_ENTER, UI_PRESS_SHORT);
    key(UI_KEY_UP, UI_PRESS_SHORT); key(UI_KEY_UP, UI_PRESS_SHORT); key(UI_KEY_UP, UI_PRESS_SHORT); /* 3 */
    key(UI_KEY_ENTER, UI_PRESS_SHORT);
    key(UI_KEY_ENTER, UI_PRESS_SHORT);                          /* lưu */
    EXPECT(last_cmd.type == UI_CMD_SET_DRY_TIME && last_cmd.u.minutes == 15 * 60 + 30, "Thoi gian say nhap tung chu so -> 15:30");

    key(UI_KEY_DOWN, UI_PRESS_SHORT);
    v.fault_text = "AP SUAT CAO"; v.state = UI_ST_FAULT; v.comp = false;
    tick(6);
    shot("04_faults");
    key(UI_KEY_ENTER, UI_PRESS_SHORT);
    EXPECT(last_cmd.type == UI_CMD_RESET_FAULT, "Trang 4: ENTER xoa loi");
    v.state = UI_ST_RUNNING; v.comp = true;
    key(UI_KEY_DOWN, UI_PRESS_SHORT);                           /* vòng về trang 1 */

    key(UI_KEY_ENTER, UI_PRESS_LONG);  shot("05_preset_menu");
    key(UI_KEY_UP, UI_PRESS_SHORT);                             /* Rau cu */
    key(UI_KEY_ENTER, UI_PRESS_SHORT);
    EXPECT(last_cmd.type == UI_CMD_SELECT_PRESET && last_cmd.u.preset.idx == 1 && v.preset == 1, "Chon che do dat san 'Rau cu'");
    shot("06_main_after_select");

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

    key(UI_KEY_ENTER, UI_PRESS_LONG);
    key(UI_KEY_DOWN, UI_PRESS_SHORT);                           /* Tu do -> dòng đồng hồ */
    key(UI_KEY_ENTER, UI_PRESS_SHORT); shot("09_edit_clock");
    for (int i = 0; i < 10; i++) key(UI_KEY_ENTER, UI_PRESS_SHORT);
    EXPECT(last_cmd.type == UI_CMD_SET_CLOCK && last_cmd.u.clock.year == 2026 && last_cmd.u.clock.hour == 14, "Chinh dong ho giu nguyen gia tri khi chi bam ENTER");

    key(UI_KEY_EXIT, UI_PRESS_SHORT);                           /* thoát menu */
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
