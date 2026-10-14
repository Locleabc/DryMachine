/**
 * @file    sensors.c
 */
#include "sensors.h"
#include <string.h>

typedef enum { SHT_IDLE = 0, SHT_BUSY } sht_phase_t;

static const sensors_cfg_t *s_cfg;
static sensors_calib_t s_cal;
static sensors_data_t  s_data;

static sht4x_t      s_sht;
static ntc_t        s_ntc[SENSORS_NTC_COUNT];

static uint32_t    s_fast_tick, s_sht_tick, s_sht_start;
static sht_phase_t s_sht_phase;
static uint8_t     s_sht_consec_err;
static bool        s_ntc_init[SENSORS_NTC_COUNT];
static float       s_ntc_filt[SENSORS_NTC_COUNT];
static uint8_t     s_ntc_bad[SENSORS_NTC_COUNT];

static const ntc_cfg_t *ntc_cfg(uint8_t i)
{
    return (i == SENSORS_NTC_CHAMBER) ? s_cfg->chamber : s_cfg->evap[i - SENSORS_NTC_EVAP0];
}

static void sht_init(void)
{
    sht4x_status_t st = SHT4X_Init(&s_sht, s_cfg->sht);
    s_data.sht_status = (uint8_t)st;
    s_data.sht_addr   = s_sht.addr;
    s_data.sht_family = (uint8_t)s_sht.family;
    if (st == SHT4X_OK) s_data.sht_serial = s_sht.serial;
}

static void sht_error(sht4x_status_t st)
{
    s_data.sht_status = (uint8_t)st;
    s_data.sht_errors++;
    if (++s_sht_consec_err >= 250) s_sht_consec_err = SENSORS_SHT_MAX_ERR;   /* không tràn, vẫn giữ trạng thái lỗi */
    if (s_sht_consec_err >= SENSORS_SHT_MAX_ERR) {
        s_data.hum.ok = false;
        s_data.hum_temp.ok = false;
    }
    if ((s_sht_consec_err % SENSORS_SHT_RECOVER) == 0) {
        if (s_cfg->bus_recover) s_cfg->bus_recover();
        sht_init();
    }
}

/* Đọc 1 NTC: lọc EMA, lỗi phải kéo dài SENSORS_NTC_MAX_ERR lần mới báo mất (giữ giá trị cũ) */
static void read_ntc(uint8_t i, float offset, sensor_val_t *out)
{
    const ntc_cfg_t *c = ntc_cfg(i);
    float ohm;
    if (!c) { out->ok = false; return; }
    uint16_t code = NTC_ReadCode(&s_ntc[i]);
    s_data.ntc_code[i] = code;
    if (!NTC_CodeToOhm(c, code, &ohm)) {                      /* hở / chập / gai nhiễu */
        s_data.ntc_glitch[i]++;
        if (s_ntc_bad[i] < 255) s_ntc_bad[i]++;
        if (s_ntc_bad[i] >= SENSORS_NTC_MAX_ERR) {
            out->ok = false;
            s_data.ntc_r[i] = 0.0f;
            s_ntc_init[i] = false;
        }
        return;
    }
    s_ntc_bad[i] = 0;
    s_data.ntc_r[i] = ohm;
    float t = NTC_OhmToTemp(ohm, c->r25, c->beta) + offset;
    if (!s_ntc_init[i]) { s_ntc_filt[i] = t; s_ntc_init[i] = true; }
    else s_ntc_filt[i] += SENSORS_NTC_ALPHA * (t - s_ntc_filt[i]);
    out->value = s_ntc_filt[i];
    out->ok = (t > -50.0f && t < 120.0f);
}

static void process_fast(void)
{
    read_ntc(SENSORS_NTC_CHAMBER, s_cal.temp_offset, &s_data.temp);
    for (uint8_t k = 0; k < SENSORS_EVAP_COUNT; k++)
        read_ntc((uint8_t)(SENSORS_NTC_EVAP0 + k), s_cal.evap_offset[k], &s_data.evap[k]);
}

static void process_sht(uint32_t now)
{
    switch (s_sht_phase) {
    case SHT_IDLE:
        if (now - s_sht_tick >= SENSORS_SHT_MS) {
            s_sht_tick = now;
            sht4x_status_t st = SHT4X_StartMeasure(&s_sht);
            if (st == SHT4X_OK) {
                s_sht_start = now;
                s_sht_phase = SHT_BUSY;
            } else {
                sht_error(st);
            }
        }
        break;

    case SHT_BUSY:
        if (now - s_sht_start >= SHT4X_BusyTimeMs(&s_sht)) {
            sht4x_result_t r;
            sht4x_status_t st = SHT4X_ReadResult(&s_sht, &r);
            if (st == SHT4X_OK) {
                s_data.sht_status     = SHT4X_OK;
                float rh = r.rh + s_cal.hum_offset;
                if (rh < 0.0f)   rh = 0.0f;
                if (rh > 100.0f) rh = 100.0f;
                s_data.hum.value      = rh;
                s_data.hum.ok         = true;
                s_data.hum_temp.value = r.temp_c;
                s_data.hum_temp.ok    = true;
                s_sht_consec_err = 0;
            } else {
                sht_error(st);
            }
            s_sht_phase = SHT_IDLE;
        }
        break;
    }
}

void Sensors_Init(const sensors_cfg_t *cfg)
{
    s_cfg = cfg;
    memset(&s_data, 0, sizeof(s_data));
    memset(&s_cal, 0, sizeof(s_cal));
    s_sht_phase = SHT_IDLE;
    s_sht_consec_err = 0;
    memset(s_ntc_init, 0, sizeof(s_ntc_init));
    memset(s_ntc_bad, 0, sizeof(s_ntc_bad));

    for (uint8_t i = 0; i < SENSORS_NTC_COUNT; i++) if (ntc_cfg(i)) NTC_Init(&s_ntc[i], ntc_cfg(i));
    if (cfg->bus_recover) cfg->bus_recover();      /* gỡ cờ BUSY kẹt của I2C F1 (sau reset giữa giao dịch) */
    sht_init();

    s_fast_tick = s_sht_tick = 0;
}

void Sensors_SetCalib(const sensors_calib_t *cal)
{
    s_cal = *cal;
}

void Sensors_Process(uint32_t now)
{
    if (now - s_fast_tick >= SENSORS_FAST_MS) {
        s_fast_tick = now;
        process_fast();
    }
    process_sht(now);
}

const sensors_data_t *Sensors_Data(void)
{
    return &s_data;
}
