/**
 * @file    presets.c
 * @note    Giá trị khởi điểm tham khảo cho máy sấy bơm nhiệt; chỉnh theo thực tế sấy.
 */
#include "presets.h"

const preset_def_t g_preset_defs[PRESET_COUNT] = {
    { "Rau thơm",       40.0f, 20.0f },
    { "Rau củ",         50.0f, 15.0f },
    { "Trái cây",       55.0f, 15.0f },
    { "Nấm",            50.0f, 18.0f },
    { "Thịt – Cá",      60.0f, 12.0f },
    { "Hạt – Nông sản", 45.0f, 12.0f },
    { "Tự do",          50.0f, 15.0f },   /* PRESET_CUSTOM */
};
