/**
 * @file    settings.h
 * @brief   Thông số vận hành – lưu Flash (page cuối) có magic + CRC32.
 *          Thêm thông số mới: thêm field vào settings_t, thêm dòng vào s_params[]
 *          trong settings.c, tăng SETTINGS_VERSION.
 */
#ifndef SETTINGS_H
#define SETTINGS_H

#include <stdint.h>
#include <stdbool.h>

#define SETTINGS_MAGIC     0x44525931UL   /* "DRY1" */
#define SETTINGS_VERSION   1

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t size;

    float temp_set;        /* °C   nhiệt độ sấy mong muốn                    */
    float temp_hyst;       /* °C   trễ nhiệt                                 */
    float hum_set;         /* %RH  độ ẩm mục tiêu                            */
    float hum_hyst;        /* %RH  trễ ẩm                                    */
    float temp_max;        /* °C   ngắt bảo vệ quá nhiệt                     */
    float p_high;          /* bar  ngắt áp cao                               */
    float p_low;           /* bar  ngắt áp thấp (sau thời gian khởi động)    */
    float press_enable;    /* 0/1  bật bảo vệ áp suất                        */
    float comp_min_off;    /* s    thời gian nghỉ tối thiểu máy nén          */
    float comp_min_on;     /* s    thời gian chạy tối thiểu máy nén          */
    float start_delay;     /* s    chạy quạt dàn lạnh trước khi bật máy nén  */
    float fan_post;        /* s    quạt chạy thêm sau khi dừng máy nén       */
    float dry_time_h;      /* h    thời gian sấy, 0 = không giới hạn         */
    float cond_fan_mode;   /* 0 = theo máy nén, 1 = theo nhiệt độ (xả nhiệt) */

    uint32_t crc;
} settings_t;

typedef struct {
    const char *name;      /* ≤ 14 ký tự, không dấu */
    const char *unit;
    uint16_t    offset;    /* offsetof trong settings_t */
    float       min, max, step;
    uint8_t     dec;       /* số chữ số thập phân khi hiển thị */
} param_desc_t;

extern settings_t g_settings;

void    Settings_Init(void);          /* nạp từ Flash, lỗi thì dùng mặc định */
void    Settings_Default(void);
bool    Settings_Save(void);
uint8_t Settings_ParamCount(void);
const param_desc_t *Settings_ParamDesc(uint8_t idx);
float   Settings_GetParam(uint8_t idx);
void    Settings_SetParam(uint8_t idx, float v);

#endif
