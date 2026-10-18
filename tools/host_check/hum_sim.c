/* Mô phỏng chương trình thử độ ẩm (App/hum_monitor.c) trên PC: SHT giả, màn hình giả lập,
 * kiểm tra thống kê / khôi phục lỗi và lưu ảnh màn hình. Build: tools/host_check/check.sh (bước 6). */
#include "hum_monitor.h"
#include "board.h"
#include "log.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

void Sim_Save(const char *path);
int  Sim_Overflow(void);

/* ---- stub phần cứng ---- */
const relay_hw_t board_relays[RLY_ID_COUNT];
const button_hw_t board_buttons[BTN_ID_COUNT];
const sht4x_cfg_t board_sht45 = { 0, SHT4X_ADDR_A, SHT4X_PREC_HIGH };
const ili9341_cfg_t board_lcd;
static uint32_t s_now = 1000;
static int relay_on, recover_cnt, init_cnt;
uint32_t Board_Millis(void) { return s_now; }
void Board_LedToggle(void) {}
void Board_LcdBacklight(bool on) { (void)on; }
void Board_LogWrite(const char *d, uint16_t n) { fwrite(d, 1, n, stdout); }
void Board_I2cRecover(void) { recover_cnt++; }
void Relay_Init(const relay_hw_t *t, uint8_t n) { (void)t; (void)n; }
void Relay_Set(uint8_t id, bool on) { (void)id; if (on) relay_on++; }
void Button_Init(const button_hw_t *t, uint8_t n) { (void)t; (void)n; }
void Button_Scan(void) {}
static int pending_key = -1;
bool Button_GetEvent(button_evt_t *e) { if (pending_key < 0) return false; e->id = (uint8_t)pending_key; e->type = BUTTON_EVT_CLICK; pending_key = -1; return true; }
void Log_Init(log_write_fn w) { (void)w; }
void Log_Printf(const char *fmt, ...) { va_list a; va_start(a, fmt); vprintf(fmt, a); va_end(a); printf("\n"); }

/* ---- SHT giả ---- */
static float fake_rh = 45.0f, fake_t = 28.3f;
static sht4x_status_t fake_err = SHT4X_OK;
sht4x_status_t SHT4X_Init(sht4x_t *d, const sht4x_cfg_t *c) { d->cfg = c; d->addr = 0x44; d->family = SHT_FAMILY_3X; d->serial = 0x1234ABCD; init_cnt++; return SHT4X_OK; }
sht4x_status_t SHT4X_StartMeasure(sht4x_t *d) { (void)d; return fake_err; }
uint32_t SHT4X_BusyTimeMs(const sht4x_t *d) { (void)d; return 20; }
sht4x_status_t SHT4X_ReadResult(sht4x_t *d, sht4x_result_t *r) { (void)d; r->rh = fake_rh; r->temp_c = fake_t; fake_rh += 0.4f; return SHT4X_OK; }

static int fails;
#define EXPECT(c, m) do { if (!(c)) { printf("FAIL: %s\n", m); fails++; } else printf("ok  : %s\n", m); } while (0)
static void run(uint32_t ms) { for (uint32_t t = 0; t < ms; t += 10) { s_now += 10; HumMon_Loop(); } }

int main(int argc, char **argv)
{
    const char *out = argc > 1 ? argv[1] : ".";
    char p[256];
    HumMon_Init();
    EXPECT(g_hum.addr == 0x44 && g_hum.family == 1 && g_hum.serial == 0x1234ABCD, "Init: nhan SHT3x @0x44");
    run(10500);
    EXPECT(g_hum.n_read >= 10 && g_hum.n_ok == g_hum.n_read && g_hum.ok, "Doc moi 1 s, tat ca OK");
    EXPECT(g_hum.rh_min == 45.0f && g_hum.rh_max > 48.0f && g_hum.hist_count == g_hum.n_ok, "Min/max/lich su");
    snprintf(p, sizeof(p), "%s/hum_ok.ppm", out); run(300); Sim_Save(p);
    EXPECT(Sim_Overflow() == 0, "Man hinh OK khong tran chu");
    fake_err = SHT4X_ERR_NACK; uint32_t ok_before = g_hum.n_ok;
    run(3500);
    EXPECT(!g_hum.ok && g_hum.n_err[SHT4X_ERR_NACK] >= 3 && g_hum.n_recover >= 1 && recover_cnt >= 2 && g_hum.n_ok == ok_before,
           "Loi NACK: dem loi, 3 lan lien tiep -> khoi phuc bus + init lai");
    snprintf(p, sizeof(p), "%s/hum_err.ppm", out); run(300); Sim_Save(p);
    EXPECT(Sim_Overflow() == 0, "Man hinh loi khong tran chu");
    fake_err = SHT4X_OK; run(1500);
    EXPECT(g_hum.ok && g_hum.consec_err == 0, "Het loi -> doc lai binh thuong");
    pending_key = BTN_ID_UP; run(100);
    EXPECT(g_hum.period_ms == 2000, "UP: chu ky 2 s");
    pending_key = BTN_ID_ENTER; run(100);
    EXPECT(g_hum.n_read == 0 && g_hum.hist_count == 0, "ENTER: xoa thong ke");
    EXPECT(relay_on == 0, "Khong bao gio dong relay");
    printf("\n%s (%d loi)\n", fails ? "HUM CO LOI" : "HUM PASS", fails);
    return fails;
}
