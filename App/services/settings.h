/**
 * @file    settings.h
 * @brief   Thông số vận hành: bảng mô tả (tên, giới hạn) + lưu Flash có magic/CRC32.
 *          Lưu trữ qua flash_store (truyền cấu hình vào Init).
 *
 *  Thêm thông số: thêm field → thêm dòng trong s_params[] (settings.c)
 *  → đặt mặc định trong Settings_Default() → tăng SETTINGS_VERSION.
 */
#ifndef SETTINGS_H
#define SETTINGS_H

#include "dryer_ctrl.h"
#include "flash_store.h"
#include "presets.h"
#include <stdbool.h>
#include <stdint.h>

#define SETTINGS_MAGIC     0x44525934UL   /* "DRY4" */
#define SETTINGS_VERSION   5      /* v5: hum_hyst (không dùng) → temp_recover, đọc được v4 */

/* Nhóm thông số (mỗi nhóm là 1 danh sách trên màn hình) */
#define SETTINGS_GROUP_PROCESS  0      /* trang "Quạt dàn nóng / chu trình" */
#define SETTINGS_GROUP_TECH     1      /* menu kỹ thuật ẩn */

/* Giới hạn điểm đặt (áp dụng cho cả chế độ đặt sẵn và tự do) */
#define SETTINGS_TEMP_MIN  30.0f
#define SETTINGS_TEMP_MAX  75.0f
#define SETTINGS_HUM_MIN   5.0f
#define SETTINGS_HUM_MAX   80.0f
#define SETTINGS_DRY_MAX_MIN  (99u * 60u + 59u)   /* 99:59 */

typedef struct {
    uint32_t       magic;
    uint16_t       version;
    uint16_t       size;
    dryer_params_t ctrl;          /* thông số điều khiển */
    float          temp_offset;   /* °C  bù PT100 */
    float          hum_offset;    /* %RH bù SHT45 */
    uint8_t        preset;        /* chế độ đang dùng: 0..PRESET_CUSTOM */
    uint8_t        reserved[3];
    float          preset_temp[PRESET_COUNT];   /* giá trị từng chế độ (sửa được) */
    float          preset_hum[PRESET_COUNT];
    uint32_t       crc;
} settings_t;

typedef struct {
    const char *name;             /* tiếng Việt UTF-8, ≤ ~190 px (font_vn16) */
    const char *unit;
    uint16_t    offset;           /* offsetof trong settings_t, kiểu float */
    float       min, max, step;
    uint8_t     dec;
    uint8_t     group;            /* SETTINGS_GROUP_* */
    const char *const *choices;   /* != NULL: hiển thị chữ thay số (giá trị = chỉ số) */
} settings_param_t;

void    Settings_Init(const flash_store_cfg_t *store);   /* nạp Flash, lỗi → mặc định */
void    Settings_Default(void);
bool    Settings_Save(void);
const settings_t *Settings_Get(void);

/* ---- Chế độ sấy & điểm đặt ---- */
void    Settings_SelectPreset(uint8_t idx);                        /* chép giá trị chế độ → điểm đặt */
bool    Settings_SetPresetValues(uint8_t idx, float temp, float hum); /* false nếu phải kẹp giới hạn */
void    Settings_SetDryTimeMin(uint16_t minutes);                  /* 0 = không giới hạn */
uint16_t Settings_DryTimeMin(void);

/* ---- Bảng thông số (chỉ số toàn cục) ---- */
uint8_t Settings_GroupCount(uint8_t group);
int     Settings_GroupIndex(uint8_t group, uint8_t n);   /* n-th thông số của nhóm → chỉ số, -1 nếu không có */
uint8_t Settings_ParamCount(void);
const settings_param_t *Settings_Param(uint8_t idx);
float   Settings_GetValue(uint8_t idx);
void    Settings_SetValue(uint8_t idx, float v);          /* tự kẹp min/max */

#endif
