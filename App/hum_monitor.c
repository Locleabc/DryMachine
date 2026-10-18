/**
 * @file    hum_monitor.c
 * @brief   Chương trình THỬ cảm biến độ ẩm – xem hum_monitor.h.
 *          Dùng lại driver sht4x / ili9341 / button / relay và board.c, không đụng bộ điều khiển máy sấy.
 */
#include "hum_monitor.h"
#include "board.h"
#include "relay.h"
#include "button.h"
#include "sht4x.h"
#include "ili9341.h"
#include "ili9341_text.h"
#include "log.h"
#include "util_fmt.h"
#include <stdio.h>
#include <string.h>

#define RECOVER_AFTER   3            /* lỗi liên tiếp → khôi phục bus I2C + init lại cảm biến */
#define EMA_ALPHA       0.1f

volatile hum_monitor_t g_hum;        /* bản sao cho Keil Watch (cập nhật sau mỗi lần đọc) */

static hum_monitor_t s;              /* bản làm việc */
static sht4x_t       s_dev;
static bool          s_busy;
static uint32_t      s_start;
static bool          s_dirty = true;

static const uint32_t s_periods[] = { 500, 1000, 2000, 5000, 10000 };
static uint8_t        s_period_idx = 1;

/* ---------------- tiện ích ---------------- */
static const char *status_text(uint8_t st)
{
    switch (st) {
    case SHT4X_OK:       return "OK";
    case SHT4X_ERR_NACK: return "NACK – không trả lời";
    case SHT4X_ERR_BUSY: return "BUSY – SDA/SCL bị kéo thấp";
    case SHT4X_ERR_CRC:  return "CRC – dữ liệu sai, nhiễu";
    case SHT4X_ERR_BUS:  return "BUS – lỗi I2C, timeout";
    default:             return "?";
    }
}

static void publish(void)
{
    g_hum = s;
    s_dirty = true;
}

static void stats_reset(void)
{
    s.rh_min = 1000.0f; s.rh_max = -1000.0f; s.rh_avg = 0.0f;
    s.n_read = s.n_ok = s.n_recover = 0;
    memset(s.n_err, 0, sizeof(s.n_err));
    s.hist_pos = s.hist_count = 0;
    memset(s.hist_rh, 0, sizeof(s.hist_rh));
    memset(s.hist_temp, 0, sizeof(s.hist_temp));
}

static void bus_scan(void)
{
    board_i2c_scan_t r;
    char a[40];
    Board_I2cScan(&r);
    s.scl_high = r.scl_high; s.sda_high = r.sda_high;
    s.scan_n = r.n_found;  memcpy(s.scan_addr, r.found, sizeof(s.scan_addr));
    s.scan_swap_n = r.n_swapped; memcpy(s.scan_swap_addr, r.swapped, sizeof(s.scan_swap_addr));
    a[0] = '\0';
    for (uint8_t i = 0; i < s.scan_n; i++) { size_t l = strlen(a); snprintf(a + l, sizeof(a) - l, " 0x%02X", (unsigned)s.scan_addr[i]); }
    Log_Printf("HUM quet I2C: SCL(PB10)=%s SDA(PB11)=%s, thay:%s%s", s.scl_high ? "cao" : "THAP",
               s.sda_high ? "cao" : "THAP", s.scan_n ? a : " khong co", s.scan_swap_n ? " | DAO DAY moi thay!" : "");
}

/* dòng kết quả quét bus cho màn hình */
static void scan_text(char *a, size_t n, uint16_t *color)
{
    *color = RGB565(235, 60, 50);
    if (!s.scl_high || !s.sda_high) {
        snprintf(a, n, "%s%s bị kéo thấp – chập dây?", s.scl_high ? "" : "SCL ",
                 s.sda_high ? "" : "SDA");
    } else if (s.scan_n) {
        size_t l;
        snprintf(a, n, "Quét I2C thấy:");
        for (uint8_t i = 0; i < s.scan_n; i++) { l = strlen(a); snprintf(a + l, n - l, " 0x%02X", (unsigned)s.scan_addr[i]); }
        *color = RGB565(60, 210, 90);
    } else if (s.scan_swap_n) {
        snprintf(a, n, "ĐẢO DÂY! Đổi SCL/SDA (thấy 0x%02X)", (unsigned)s.scan_swap_addr[0]);
    } else {
        snprintf(a, n, "Quét I2C: không có thiết bị nào");
    }
}

static void sensor_init(void)
{
    bus_scan();                                          /* quét + gỡ cờ BUSY kẹt của I2C F1 */
    sht4x_status_t st = SHT4X_Init(&s_dev, &board_sht45);
    s.status = (uint8_t)st;
    s.status_text = status_text(s.status);
    s.addr   = s_dev.addr;
    s.family = (uint8_t)s_dev.family;
    s.serial = (st == SHT4X_OK) ? s_dev.serial : 0;
    s_busy = false;
    Log_Printf("HUM init: %s @0x%02X serial %08lX st=%s", s.family ? "SHT3x" : "SHT4x",
               (unsigned)s.addr, (unsigned long)s.serial, s.status_text);
}

static void on_error(sht4x_status_t st)
{
    s.ok = false;
    s.status = (uint8_t)st;
    s.status_text = status_text(s.status);
    if (s.status < 5) s.n_err[s.status]++;
    if (s.consec_err < 255) s.consec_err++;
    Log_Printf("HUM #%lu LOI %s (lien tiep %u)", (unsigned long)s.n_read, s.status_text, (unsigned)s.consec_err);
    if (s.consec_err % RECOVER_AFTER == 0) {
        s.n_recover++;
        Log_Printf("HUM khoi phuc bus I2C + init lai (lan %lu)", (unsigned long)s.n_recover);
        sensor_init();
    }
}

static void on_result(const sht4x_result_t *r)
{
    char a[12], b[12], c[12], d[12];
    s.ok = true;
    s.status = SHT4X_OK;
    s.status_text = status_text(SHT4X_OK);
    s.rh = r->rh;
    s.temp = r->temp_c;
    s.n_ok++;
    s.consec_err = 0;
    if (s.rh < s.rh_min) s.rh_min = s.rh;
    if (s.rh > s.rh_max) s.rh_max = s.rh;
    s.rh_avg = (s.n_ok == 1) ? s.rh : s.rh_avg + EMA_ALPHA * (s.rh - s.rh_avg);
    s.hist_rh[s.hist_pos]   = s.rh;
    s.hist_temp[s.hist_pos] = s.temp;
    s.hist_pos = (uint8_t)((s.hist_pos + 1) % HUM_HIST_LEN);
    if (s.hist_count < HUM_HIST_LEN) s.hist_count++;
    Log_Printf("HUM #%lu RH=%s%% T=%sC min=%s max=%s ok=%lu loi=%lu",
               (unsigned long)s.n_read, Fmt_Float(a, sizeof(a), s.rh, 2), Fmt_Float(b, sizeof(b), s.temp, 2),
               Fmt_Float(c, sizeof(c), s.rh_min, 1), Fmt_Float(d, sizeof(d), s.rh_max, 1),
               (unsigned long)s.n_ok, (unsigned long)(s.n_read - s.n_ok));
}

/* ---------------- màn hình ---------------- */
#define F_TXT  (&font_vn16)
#define F_NUM  (&font_num)
#define UC_HEAD  RGB565(18, 42, 84)
#define UC_FOOT  RGB565(26, 30, 36)
#define UC_LABEL RGB565(140, 150, 162)
#define UC_HUM   RGB565(90, 180, 255)
#define UC_TEMP  RGB565(255, 150, 50)
#define UC_OK    RGB565(60, 210, 90)
#define UC_ERR   RGB565(235, 60, 50)

static void draw_static(void)
{
    ILI9341_FillScreen(C_BLACK);
    ILI9341_FillRect(0, 0, TFT_WIDTH, 27, UC_HEAD);
    Text_Box(8, 2, 184, "THỬ CẢM BIẾN ĐỘ ẨM", F_TXT, C_WHITE, UC_HEAD, TEXT_LEFT);
    Text_Box(8, 30, 150, "Độ ẩm (%RH)", F_TXT, UC_LABEL, C_BLACK, TEXT_LEFT);
    Text_Box(168, 30, 144, "Nhiệt độ SHT (°C)", F_TXT, UC_LABEL, C_BLACK, TEXT_LEFT);
    ILI9341_FillRect(0, 217, TFT_WIDTH, 23, UC_FOOT);
}

static void draw_values(void)
{
    char a[64], b[12], c[12], d[12];

    snprintf(a, sizeof(a), "%s @0x%02X", s.family ? "SHT3x" : "SHT4x", (unsigned)s.addr);
    Text_Box(192, 2, TFT_WIDTH - 200, a, F_TXT, C_WHITE, UC_HEAD, TEXT_RIGHT);

    if (s.n_ok) {
        snprintf(a, sizeof(a), "%s%%", Fmt_Float(b, sizeof(b), s.rh, 1));
        Text_Box(8, 52, 152, a, F_NUM, s.ok ? UC_HUM : UC_LABEL, C_BLACK, TEXT_LEFT);
        snprintf(a, sizeof(a), "%s°", Fmt_Float(b, sizeof(b), s.temp, 1));
        Text_Box(168, 52, 144, a, F_NUM, s.ok ? UC_TEMP : UC_LABEL, C_BLACK, TEXT_LEFT);
    } else {
        Text_Box(8, 52, 152, "--.-%", F_NUM, UC_LABEL, C_BLACK, TEXT_LEFT);
        Text_Box(168, 52, 144, "--.-°", F_NUM, UC_LABEL, C_BLACK, TEXT_LEFT);
    }

    snprintf(a, sizeof(a), "Trạng thái: %s", s.status_text ? s.status_text : "?");
    Text_Box(8, 92, TFT_WIDTH - 16, a, F_TXT, s.ok ? UC_OK : UC_ERR, C_BLACK, TEXT_LEFT);

    if (s.n_ok) snprintf(a, sizeof(a), "Min %s · Max %s · TB %s %%", Fmt_Float(b, sizeof(b), s.rh_min, 1),
                         Fmt_Float(c, sizeof(c), s.rh_max, 1), Fmt_Float(d, sizeof(d), s.rh_avg, 1));
    else        snprintf(a, sizeof(a), "Min -- · Max -- · TB --");
    Text_Box(8, 116, TFT_WIDTH - 16, a, F_TXT, C_WHITE, C_BLACK, TEXT_LEFT);

    snprintf(a, sizeof(a), "Đọc %lu · OK %lu · Lỗi %lu · KP %lu", (unsigned long)s.n_read, (unsigned long)s.n_ok,
             (unsigned long)(s.n_read - s.n_ok), (unsigned long)s.n_recover);
    Text_Box(8, 140, TFT_WIDTH - 16, a, F_TXT, UC_LABEL, C_BLACK, TEXT_LEFT);

    /* 3 lần đọc gần nhất (mới nhất bên trái) */
    for (uint8_t row = 0; row < 1; row++) {
        a[0] = '\0';
        for (uint8_t k = 0; k < 3; k++) {
            uint8_t n = (uint8_t)(row * 3 + k);
            if (n >= s.hist_count) break;
            uint8_t idx = (uint8_t)((s.hist_pos + HUM_HIST_LEN - 1 - n) % HUM_HIST_LEN);
            size_t len = strlen(a);
            snprintf(a + len, sizeof(a) - len, "%s%s%%", k ? "   " : (row ? "        " : "Gần đây: "),
                     Fmt_Float(b, sizeof(b), s.hist_rh[idx], 1));
        }
        Text_Box(8, (int16_t)(166 + row * 23), TFT_WIDTH - 16, a, F_TXT, UC_HUM, C_BLACK, TEXT_LEFT);
    }

    {
        uint16_t col;
        scan_text(a, sizeof(a), &col);
        Text_Box(8, 189, TFT_WIDTH - 16, a, F_TXT, col, C_BLACK, TEXT_LEFT);
    }

    snprintf(a, sizeof(a), "ENTER xoá TK · EXIT init · UP/DN %s s",
             Fmt_Float(b, sizeof(b), (float)s.period_ms / 1000.0f, 1));
    Text_Box(6, 217, TFT_WIDTH - 12, a, F_TXT, UC_LABEL, UC_FOOT, TEXT_LEFT);
}

/* ---------------- nút ---------------- */
static void handle_keys(void)
{
    button_evt_t e;
    while (Button_GetEvent(&e)) {
        if (e.type == BUTTON_EVT_LONG) continue;
        switch (e.id) {
        case BTN_ID_ENTER: stats_reset(); Log_Printf("HUM xoa thong ke"); break;
        case BTN_ID_EXIT:  sensor_init(); break;
        case BTN_ID_UP:    if (s_period_idx + 1u < sizeof(s_periods) / sizeof(s_periods[0])) s_period_idx++; break;
        case BTN_ID_DOWN:  if (s_period_idx > 0) s_period_idx--; break;
        default: break;
        }
        s.period_ms = s_periods[s_period_idx];
        publish();
    }
}

/* ---------------- API ---------------- */
void HumMon_Init(void)
{
    Relay_Init(board_relays, RLY_ID_COUNT);          /* mọi relay NHẢ – chương trình thử không chạy máy */
    Button_Init(board_buttons, BTN_ID_COUNT);
    Log_Init(Board_LogWrite);
    Board_LcdBacklight(true);
    ILI9341_Init(&board_lcd);

    memset(&s, 0, sizeof(s));
    stats_reset();
    s.period_ms = s_periods[s_period_idx];
    Log_Printf("=== HUM MONITOR (nhanh test/hum-monitor) – relay luon nha ===");
    sensor_init();
    draw_static();
    publish();
}

void HumMon_Loop(void)
{
    static uint32_t scan_tick, draw_tick, led_tick;
    uint32_t now = Board_Millis();

    if (now - scan_tick >= BUTTON_SCAN_MS) { scan_tick = now; Button_Scan(); handle_keys(); }

    if (!s_busy && now - s.last_ms >= s.period_ms) {
        s.last_ms = now;
        s.n_read++;
        sht4x_status_t st = SHT4X_StartMeasure(&s_dev);
        if (st == SHT4X_OK) { s_busy = true; s_start = now; }
        else { on_error(st); publish(); }
    } else if (s_busy && now - s_start >= SHT4X_BusyTimeMs(&s_dev)) {
        sht4x_result_t r;
        sht4x_status_t st = SHT4X_ReadResult(&s_dev, &r);
        s_busy = false;
        if (st == SHT4X_OK) on_result(&r); else on_error(st);
        publish();
    }

    if (s_dirty && now - draw_tick >= 200) {          /* vẽ lại tối đa 5 lần/s */
        draw_tick = now;
        s_dirty = false;
        draw_values();
    }
    if (now - led_tick >= 500) { led_tick = now; Board_LedToggle(); }   /* LED nháy = chương trình đang chạy */
}
