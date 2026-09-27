/**
 * @file    presets.c
 * @note    Giá trị khởi điểm tham khảo cho máy sấy bơm nhiệt; chỉnh theo thực tế sấy.
 */
#include "presets.h"

const preset_def_t g_preset_defs[PRESET_COUNT] = {
    { "Rau thom",   40.0f, 20.0f },
    { "Rau cu",     50.0f, 15.0f },
    { "Trai cay",   55.0f, 15.0f },
    { "Nam",        50.0f, 18.0f },
    { "Thit - Ca",  60.0f, 12.0f },
    { "Hat - Nong", 45.0f, 12.0f },
    { "Tu do",      50.0f, 15.0f },   /* PRESET_CUSTOM */
};
