/**
 * @file    settings.c
 */
#include "settings.h"
#include "app_config.h"
#include <stddef.h>
#include <string.h>

settings_t g_settings;

#define P(field, name, unit, mn, mx, st, dc) \
    { name, unit, (uint16_t)offsetof(settings_t, field), mn, mx, st, dc }

static const param_desc_t s_params[] = {
    P(temp_set,      "Nhiet do dat",  "C",   30.0f,  75.0f, 0.5f, 1),
    P(temp_hyst,     "Tre nhiet",     "C",    0.5f,  10.0f, 0.5f, 1),
    P(hum_set,       "Do am dat",     "%",    5.0f,  80.0f, 1.0f, 0),
    P(hum_hyst,      "Tre am",        "%",    1.0f,  20.0f, 1.0f, 0),
    P(temp_max,      "Qua nhiet",     "C",   50.0f,  95.0f, 1.0f, 0),
    P(p_high,        "Ap cao ngat",   "bar",  5.0f,  45.0f, 0.5f, 1),
    P(p_low,         "Ap thap ngat",  "bar",  0.0f,  10.0f, 0.1f, 1),
    P(press_enable,  "Bao ve ap",     "",     0.0f,   1.0f, 1.0f, 0),
    P(comp_min_off,  "MN nghi min",   "s",   30.0f, 600.0f, 10.0f, 0),
    P(comp_min_on,   "MN chay min",   "s",   10.0f, 600.0f, 10.0f, 0),
    P(start_delay,   "Tre khoi dong", "s",    0.0f, 120.0f, 5.0f, 0),
    P(fan_post,      "Quat chay them","s",    0.0f, 300.0f, 5.0f, 0),
    P(dry_time_h,    "Thoi gian say", "h",    0.0f,  72.0f, 0.5f, 1),
    P(cond_fan_mode, "Quat nong mode","",     0.0f,   1.0f, 1.0f, 0),
};

static uint32_t crc32_calc(const uint8_t *p, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFFUL;
    while (len--) {
        crc ^= *p++;
        for (int i = 0; i < 8; i++) crc = (crc & 1) ? (crc >> 1) ^ 0xEDB88320UL : (crc >> 1);
    }
    return ~crc;
}

static uint32_t settings_crc(const settings_t *s)
{
    return crc32_calc((const uint8_t *)s, offsetof(settings_t, crc));
}

void Settings_Default(void)
{
    memset(&g_settings, 0, sizeof(g_settings));
    g_settings.magic         = SETTINGS_MAGIC;
    g_settings.version       = SETTINGS_VERSION;
    g_settings.size          = sizeof(settings_t);
    g_settings.temp_set      = 55.0f;
    g_settings.temp_hyst     = 2.0f;
    g_settings.hum_set       = 15.0f;
    g_settings.hum_hyst      = 3.0f;
    g_settings.temp_max      = 75.0f;
    g_settings.p_high        = 30.0f;   /* TODO: chỉnh theo loại gas và vị trí cảm biến */
    g_settings.p_low         = 1.0f;
    g_settings.press_enable  = 1.0f;
    g_settings.comp_min_off  = 180.0f;  /* 3 phút chống khởi động liên tục */
    g_settings.comp_min_on   = 60.0f;
    g_settings.start_delay   = 10.0f;
    g_settings.fan_post      = 60.0f;
    g_settings.dry_time_h    = 0.0f;
    g_settings.cond_fan_mode = 0.0f;
    g_settings.crc           = settings_crc(&g_settings);
}

void Settings_Init(void)
{
    const settings_t *f = (const settings_t *)SETTINGS_FLASH_ADDR;
    if (f->magic == SETTINGS_MAGIC && f->version == SETTINGS_VERSION &&
        f->size == sizeof(settings_t) && f->crc == settings_crc(f)) {
        memcpy(&g_settings, f, sizeof(settings_t));
    } else {
        Settings_Default();
    }
    /* Kẹp giá trị về giới hạn cho chắc chắn */
    for (uint8_t i = 0; i < Settings_ParamCount(); i++) {
        Settings_SetParam(i, Settings_GetParam(i));
    }
}

bool Settings_Save(void)
{
    g_settings.magic   = SETTINGS_MAGIC;
    g_settings.version = SETTINGS_VERSION;
    g_settings.size    = sizeof(settings_t);
    g_settings.crc     = settings_crc(&g_settings);

    FLASH_EraseInitTypeDef er = {0};
    uint32_t page_err = 0;
    er.TypeErase   = FLASH_TYPEERASE_PAGES;
    er.PageAddress = SETTINGS_FLASH_ADDR;
    er.NbPages     = 1;

    bool ok = true;
    HAL_FLASH_Unlock();
    if (HAL_FLASHEx_Erase(&er, &page_err) != HAL_OK) ok = false;

    const uint16_t *src = (const uint16_t *)&g_settings;
    for (uint32_t i = 0; ok && i < (sizeof(settings_t) + 1) / 2; i++) {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, SETTINGS_FLASH_ADDR + i * 2, src[i]) != HAL_OK) {
            ok = false;
        }
    }
    HAL_FLASH_Lock();

    return ok && (memcmp((const void *)SETTINGS_FLASH_ADDR, &g_settings, sizeof(settings_t)) == 0);
}

uint8_t Settings_ParamCount(void)
{
    return (uint8_t)ARRAY_LEN(s_params);
}

const param_desc_t *Settings_ParamDesc(uint8_t idx)
{
    return (idx < ARRAY_LEN(s_params)) ? &s_params[idx] : NULL;
}

float Settings_GetParam(uint8_t idx)
{
    const param_desc_t *d = Settings_ParamDesc(idx);
    if (!d) return 0.0f;
    float v;
    memcpy(&v, (const uint8_t *)&g_settings + d->offset, sizeof(float));
    return v;
}

void Settings_SetParam(uint8_t idx, float v)
{
    const param_desc_t *d = Settings_ParamDesc(idx);
    if (!d) return;
    if (v < d->min) v = d->min;
    if (v > d->max) v = d->max;
    memcpy((uint8_t *)&g_settings + d->offset, &v, sizeof(float));
}
