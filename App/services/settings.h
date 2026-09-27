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
#include <stdbool.h>
#include <stdint.h>

#define SETTINGS_MAGIC     0x44525932UL   /* "DRY2" */
#define SETTINGS_VERSION   2

typedef struct {
    uint32_t       magic;
    uint16_t       version;
    uint16_t       size;
    dryer_params_t ctrl;          /* thông số điều khiển */
    float          temp_offset;   /* °C  bù PT100 */
    float          hum_offset;    /* %RH bù SHT45 */
    uint32_t       crc;
} settings_t;

typedef struct {
    const char *name;             /* ≤ 14 ký tự, không dấu */
    const char *unit;
    uint16_t    offset;           /* offsetof trong settings_t, kiểu float */
    float       min, max, step;
    uint8_t     dec;
} settings_param_t;

void    Settings_Init(const flash_store_cfg_t *store);   /* nạp Flash, lỗi → mặc định */
void    Settings_Default(void);
bool    Settings_Save(void);
const settings_t *Settings_Get(void);

uint8_t Settings_ParamCount(void);
const settings_param_t *Settings_Param(uint8_t idx);
float   Settings_GetValue(uint8_t idx);
void    Settings_SetValue(uint8_t idx, float v);          /* tự kẹp min/max */

#endif
