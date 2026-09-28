/**
 * @file    presets.h
 * @brief   Các chế độ sấy đặt sẵn (tên + nhiệt độ/độ ẩm mặc định).
 *          Giá trị thực tế đang dùng được lưu trong settings (người dùng sửa được).
 *          Muốn đổi tên hoặc giá trị mặc định: sửa bảng trong presets.c.
 */
#ifndef PRESETS_H
#define PRESETS_H

#include <stdint.h>

#define PRESET_FIXED_COUNT   6                       /* 6 chế độ đặt sẵn */
#define PRESET_CUSTOM        PRESET_FIXED_COUNT      /* chỉ số chế độ "Tu do" */
#define PRESET_COUNT         (PRESET_FIXED_COUNT + 1)

typedef struct {
    const char *name;        /* tiếng Việt UTF-8, ≤ ~180 px (font_vn16) */
    float       temp;        /* °C  */
    float       hum;         /* %RH */
} preset_def_t;

extern const preset_def_t g_preset_defs[PRESET_COUNT];

#endif
