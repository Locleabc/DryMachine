/**
 * @file    settings.c
 */
#include "settings.h"
#include <stddef.h>
#include <string.h>

#define P(field, name, unit, mn, mx, st, dc) \
    { name, unit, (uint16_t)offsetof(settings_t, field), mn, mx, st, dc }

static const settings_param_t s_params[] = {
    P(ctrl.temp_set,      "Nhiet do dat",  "C",   30.0f,  75.0f, 0.5f,  1),
    P(ctrl.temp_hyst,     "Tre nhiet",     "C",    0.5f,  10.0f, 0.5f,  1),
    P(ctrl.hum_set,       "Do am dat",     "%",    5.0f,  80.0f, 1.0f,  0),
    P(ctrl.hum_hyst,      "Tre am",        "%",    1.0f,  20.0f, 1.0f,  0),
    P(ctrl.temp_max,      "Qua nhiet",     "C",   50.0f,  95.0f, 1.0f,  0),
    P(ctrl.p_high,        "Ap cao ngat",   "bar",  5.0f,  45.0f, 0.5f,  1),
    P(ctrl.p_low,         "Ap thap ngat",  "bar",  0.0f,  10.0f, 0.1f,  1),
    P(ctrl.press_enable,  "Bao ve ap",     "",     0.0f,   1.0f, 1.0f,  0),
    P(ctrl.comp_min_off,  "MN nghi min",   "s",   30.0f, 600.0f, 10.0f, 0),
    P(ctrl.comp_min_on,   "MN chay min",   "s",   10.0f, 600.0f, 10.0f, 0),
    P(ctrl.start_delay,   "Tre khoi dong", "s",    0.0f, 120.0f, 5.0f,  0),
    P(ctrl.fan_post,      "Quat chay them","s",    0.0f, 300.0f, 5.0f,  0),
    P(ctrl.dry_time_h,    "Thoi gian say", "h",    0.0f,  72.0f, 0.5f,  1),
    P(ctrl.cond_fan_mode, "Quat nong mode","",     0.0f,   1.0f, 1.0f,  0),
    P(temp_offset,        "Bu nhiet PT100","C",   -5.0f,   5.0f, 0.1f,  1),
    P(hum_offset,         "Bu do am",      "%",  -10.0f,  10.0f, 0.5f,  1),
};

#define PARAM_COUNT  (sizeof(s_params) / sizeof(s_params[0]))

static settings_t s_set;
static const flash_store_cfg_t *s_store;

static uint32_t crc32_calc(const uint8_t *p, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFFUL;
    while (len--) {
        crc ^= *p++;
        for (int i = 0; i < 8; i++) crc = (crc & 1) ? (crc >> 1) ^ 0xEDB88320UL : (crc >> 1);
    }
    return ~crc;
}

static uint32_t calc_crc(const settings_t *s)
{
    return crc32_calc((const uint8_t *)s, offsetof(settings_t, crc));
}

static void stamp(settings_t *s)
{
    s->magic   = SETTINGS_MAGIC;
    s->version = SETTINGS_VERSION;
    s->size    = sizeof(settings_t);
    s->crc     = calc_crc(s);
}

void Settings_Default(void)
{
    memset(&s_set, 0, sizeof(s_set));
    DryerCtrl_DefaultParams(&s_set.ctrl);
    s_set.temp_offset = 0.0f;
    s_set.hum_offset  = 0.0f;
    stamp(&s_set);
}

void Settings_Init(const flash_store_cfg_t *store)
{
    settings_t tmp;
    s_store = store;
    if (store && FlashStore_Read(store, &tmp, sizeof(tmp)) &&
        tmp.magic == SETTINGS_MAGIC && tmp.version == SETTINGS_VERSION &&
        tmp.size == sizeof(settings_t) && tmp.crc == calc_crc(&tmp)) {
        s_set = tmp;
    } else {
        Settings_Default();
    }
    for (uint8_t i = 0; i < PARAM_COUNT; i++) Settings_SetValue(i, Settings_GetValue(i));  /* kẹp giới hạn */
}

bool Settings_Save(void)
{
    stamp(&s_set);
    return s_store && FlashStore_Write(s_store, &s_set, sizeof(s_set));
}

const settings_t *Settings_Get(void)
{
    return &s_set;
}

uint8_t Settings_ParamCount(void)
{
    return (uint8_t)PARAM_COUNT;
}

const settings_param_t *Settings_Param(uint8_t idx)
{
    return (idx < PARAM_COUNT) ? &s_params[idx] : NULL;
}

float Settings_GetValue(uint8_t idx)
{
    float v = 0.0f;
    if (idx < PARAM_COUNT) memcpy(&v, (const uint8_t *)&s_set + s_params[idx].offset, sizeof(v));
    return v;
}

void Settings_SetValue(uint8_t idx, float v)
{
    if (idx >= PARAM_COUNT) return;
    const settings_param_t *d = &s_params[idx];
    if (v < d->min) v = d->min;
    if (v > d->max) v = d->max;
    memcpy((uint8_t *)&s_set + d->offset, &v, sizeof(v));
}
