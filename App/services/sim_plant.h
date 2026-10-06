/**
 * @file    sim_plant.h
 * @brief   Mô hình buồng sấy đơn giản để CHẠY GIẢ LẬP khi chưa có cảm biến (thuần C).
 *          Máy nén chạy → nóng lên, khô đi; quạt dàn nóng cấp cao → xả bớt nhiệt;
 *          máy nén tắt → nguội dần về nhiệt độ môi trường, ẩm tăng lại từ vật sấy.
 *          Dàn lạnh: MN chạy → ~2 °C (đóng băng: −8 °C), tắt → ấm dần về nhiệt độ buồng.
 *          Hệ số chọn để một chu trình mặc định chạy vài giờ (×1), vài phút (×60).
 */
#ifndef SIM_PLANT_H
#define SIM_PLANT_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    float temp;        /* °C  buồng sấy */
    float hum;         /* %RH buồng sấy */
    float evap;        /* °C  dàn lạnh (NTC) */
    bool  icing;       /* giả lập đóng băng: MN chạy kéo dàn lạnh xuống −8 °C */
    float amb_temp;    /* °C  môi trường */
    float amb_hum;     /* %RH môi trường */
} sim_plant_t;

void SimPlant_Init(sim_plant_t *s, float temp, float hum);
/* dt_s: thời gian mô phỏng (giây, đã nhân hệ số tua nhanh) */
void SimPlant_Step(sim_plant_t *s, float dt_s, bool comp, uint8_t fan_level, bool fan_evap);

#endif
