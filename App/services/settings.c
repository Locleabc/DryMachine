/**
 * @file    settings.c
 */
#include "settings.h"
#include <stddef.h>
#include <string.h>

#define P(grp, field, name, unit, mn, mx, st, dc, ch) \
    { name, unit, (uint16_t)offsetof(settings_t, field), mn, mx, st, dc, grp, ch }

static const char *const s_mode_ch[]  = { "Tự động", "Thủ công" };
static const char *const s_onoff_ch[] = { "Tắt", "Bật" };

#define PR SETTINGS_GROUP_PROCESS
#define TE SETTINGS_GROUP_TECH

static const settings_param_t s_params[] = {
    /* ---- Trang "Quạt dàn nóng / chu trình" ---- */
    P(PR, ctrl.mode,          "Chế độ điều khiển",   "",     0.0f,   1.0f, 1.0f,  0, s_mode_ch),
    P(PR, ctrl.auto_fan,      "Tự động: cấp quạt",   "",     1.0f,   5.0f, 1.0f,  0, NULL),
    P(PR, ctrl.stage_fan[0],  "GĐ1: cấp quạt",       "",     1.0f,   5.0f, 1.0f,  0, NULL),
    P(PR, ctrl.start_delay,   "GĐ1: quạt chạy trước","s",    0.0f, 300.0f, 5.0f,  0, NULL),
    P(PR, ctrl.stage_fan[1],  "GĐ2: cấp quạt",       "",     1.0f,   5.0f, 1.0f,  0, NULL),
    P(PR, ctrl.stage_fan[2],  "GĐ3: cấp quạt",       "",     1.0f,   5.0f, 1.0f,  0, NULL),
    P(PR, ctrl.gd3_min,       "GĐ3: thời gian",      "phút", 0.0f, 999.0f, 5.0f,  0, NULL),
    P(PR, ctrl.stage_fan[3],  "GĐ4: cấp quạt",       "",     1.0f,   5.0f, 1.0f,  0, NULL),
    P(PR, ctrl.gd4_min,       "GĐ4: thời gian",      "phút", 0.0f, 999.0f, 5.0f,  0, NULL),
    P(PR, ctrl.stage_fan[4],  "GĐ5: cấp quạt",       "",     1.0f,   5.0f, 1.0f,  0, NULL),
    P(PR, ctrl.end_temp,      "GĐ5: nhiệt độ dừng",  "°C",  20.0f,  60.0f, 1.0f,  0, NULL),
    P(PR, ctrl.comp_min_off,  "Máy nén chờ bật lại", "s",   10.0f, 600.0f, 5.0f,  0, NULL),
    P(PR, ctrl.temp_max,      "Nhiệt độ bảo vệ",     "°C",  50.0f,  95.0f, 1.0f,  0, NULL),
    P(PR, ctrl.temp_recover,  "Quá nhiệt: nguội tới","°C",  20.0f,  60.0f, 1.0f,  0, NULL),
    /* ---- Menu kỹ thuật (ẩn) ---- */
    P(TE, ctrl.temp_hyst,     "Trễ nhiệt",           "°C",   0.5f,  10.0f, 0.5f,  1, NULL),
    P(TE, ctrl.p_high,        "Ngắt áp cao",         "bar",  5.0f,  45.0f, 0.5f,  1, NULL),
    P(TE, ctrl.p_low,         "Ngắt áp thấp",        "bar",  0.0f,  10.0f, 0.1f,  1, NULL),
    P(TE, ctrl.press_enable,  "Bảo vệ áp suất",      "",     0.0f,   1.0f, 1.0f,  0, s_onoff_ch),
    P(TE, ctrl.comp_min_on,   "Máy nén chạy min",    "s",   10.0f, 600.0f, 10.0f, 0, NULL),
    P(TE, ctrl.fan_post,      "Quạt chạy thêm",      "s",    0.0f, 300.0f, 5.0f,  0, NULL),
    P(TE, temp_offset,        "Bù nhiệt PT100",      "°C",  -5.0f,   5.0f, 0.1f,  1, NULL),
    P(TE, hum_offset,         "Bù độ ẩm",            "%",  -10.0f,  10.0f, 0.5f,  1, NULL),
    P(TE, screen_off_min,     "Tắt màn hình sau",    "phút", 0.0f,  60.0f, 1.0f,  0, NULL),   /* 0 = luôn sáng */
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
    s_set.screen_off_min = SETTINGS_SCREEN_OFF_DEF;
    for (uint8_t i = 0; i < PRESET_COUNT; i++) {
        s_set.preset_temp[i] = g_preset_defs[i].temp;
        s_set.preset_hum[i]  = g_preset_defs[i].hum;
    }
    Settings_SelectPreset(PRESET_CUSTOM);
    stamp(&s_set);
}

/* v4/v5: cấu trúc = tiền tố của v6 tới hết preset_hum, CRC nằm ngay sau (đúng chỗ screen_off_min) */
#define SETTINGS_V5_BODY  offsetof(settings_t, screen_off_min)

static bool load_legacy(const settings_t *tmp)
{
    uint32_t crc;
    if (tmp->magic != SETTINGS_MAGIC || (tmp->version != 4 && tmp->version != 5) ||
        tmp->size != SETTINGS_V5_BODY + sizeof(uint32_t)) return false;
    memcpy(&crc, (const uint8_t *)tmp + SETTINGS_V5_BODY, sizeof(crc));
    if (crc != crc32_calc((const uint8_t *)tmp, SETTINGS_V5_BODY)) return false;
    s_set = *tmp;
    if (tmp->version == 4) s_set.ctrl.temp_recover = 30.0f;   /* v4: ô này là hum_hyst cũ – giữ các cài đặt khác */
    s_set.screen_off_min = SETTINGS_SCREEN_OFF_DEF;
    stamp(&s_set);
    return true;
}

void Settings_Init(const flash_store_cfg_t *store)
{
    settings_t tmp;
    s_store = store;
    bool ok = store && FlashStore_Read(store, &tmp, sizeof(tmp));
    if (ok && tmp.magic == SETTINGS_MAGIC && tmp.version == SETTINGS_VERSION &&
        tmp.size == sizeof(settings_t) && tmp.crc == calc_crc(&tmp)) {
        s_set = tmp;
    } else if (!(ok && load_legacy(&tmp))) {
        Settings_Default();
    }
    for (uint8_t i = 0; i < PARAM_COUNT; i++) Settings_SetValue(i, Settings_GetValue(i));  /* kẹp giới hạn */
    for (uint8_t i = 0; i < PRESET_COUNT; i++) Settings_SetPresetValues(i, s_set.preset_temp[i], s_set.preset_hum[i]);
    if (s_set.preset >= PRESET_COUNT) s_set.preset = PRESET_CUSTOM;
    Settings_SelectPreset(s_set.preset);
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

static float clampf(float v, float lo, float hi, bool *clamped)
{
    if (v < lo) { *clamped = true; return lo; }
    if (v > hi) { *clamped = true; return hi; }
    return v;
}

void Settings_SelectPreset(uint8_t idx)
{
    if (idx >= PRESET_COUNT) return;
    s_set.preset = idx;
    s_set.ctrl.temp_set = s_set.preset_temp[idx];
    s_set.ctrl.hum_set  = s_set.preset_hum[idx];
}

bool Settings_SetPresetValues(uint8_t idx, float temp, float hum)
{
    bool clamped = false;
    if (idx >= PRESET_COUNT) return false;
    s_set.preset_temp[idx] = clampf(temp, SETTINGS_TEMP_MIN, SETTINGS_TEMP_MAX, &clamped);
    s_set.preset_hum[idx]  = clampf(hum,  SETTINGS_HUM_MIN,  SETTINGS_HUM_MAX,  &clamped);
    if (idx == s_set.preset) Settings_SelectPreset(idx);   /* đang dùng → áp dụng ngay */
    return !clamped;
}

void Settings_SetDryTimeMin(uint16_t minutes)
{
    if (minutes > SETTINGS_DRY_MAX_MIN) minutes = SETTINGS_DRY_MAX_MIN;
    s_set.ctrl.dry_time_h = (float)minutes / 60.0f;
}

uint16_t Settings_DryTimeMin(void)
{
    return (uint16_t)(s_set.ctrl.dry_time_h * 60.0f + 0.5f);
}

uint8_t Settings_GroupCount(uint8_t group)
{
    uint8_t n = 0;
    for (uint8_t i = 0; i < PARAM_COUNT; i++) if (s_params[i].group == group) n++;
    return n;
}

int Settings_GroupIndex(uint8_t group, uint8_t n)
{
    for (uint8_t i = 0; i < PARAM_COUNT; i++) {
        if (s_params[i].group == group && n-- == 0) return i;
    }
    return -1;
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
