/**
 * @file    sensors.c
 */
#include "sensors.h"
#include <string.h>

typedef enum { SHT_IDLE = 0, SHT_BUSY } sht_phase_t;

static const sensors_cfg_t *s_cfg;
static sensors_calib_t s_cal;
static sensors_data_t  s_data;

static max31865_t   s_pt100;
static sht4x_t      s_sht;
static press_analog_t s_press;

static uint32_t    s_fast_tick, s_sht_tick, s_sht_start;
static sht_phase_t s_sht_phase;
static uint8_t     s_sht_consec_err;
static bool        s_press_init;
static float       s_press_filt;

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

static void process_fast(void)
{
    max31865_result_t r;
    MAX31865_Read(&s_pt100, &r);
    s_data.temp.ok     = r.ok;
    s_data.temp.value  = r.temp_c + s_cal.temp_offset;
    s_data.pt100_fault = r.fault;

    float v = PressAnalog_ReadVolt(&s_press);
    if (!s_press_init) { s_press_filt = v; s_press_init = true; }
    else s_press_filt += SENSORS_PRESS_ALPHA * (v - s_press_filt);
    s_data.press_volt = s_press_filt;
    s_data.press.ok = PressAnalog_VoltToBar(s_cfg->press, s_press_filt, &s_data.press.value);
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
    s_press_init = false;

    MAX31865_Init(&s_pt100, cfg->pt100);
    PressAnalog_Init(&s_press, cfg->press);
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
