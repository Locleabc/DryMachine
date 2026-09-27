/**
 * @file    app_config.h
 * @brief   Cấu hình chung cho ứng dụng DryMachine.
 *          Mọi hằng số phần cứng / hiệu chuẩn tập trung tại đây.
 */
#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include "main.h"          /* HAL + macro chân do CubeMX sinh (User Label) */
#include <stdint.h>
#include <stdbool.h>

/* ---------------- Handle ngoại vi (định nghĩa trong main.c) --------------- */
extern SPI_HandleTypeDef  hspi1;
extern SPI_HandleTypeDef  hspi2;
extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;
extern ADC_HandleTypeDef  hadc1;

#define TFT_SPI            (&hspi1)
#define MAX31865_SPI       (&hspi2)
#define RS485_UART         (&huart2)
#define DEBUG_UART         (&huart1)
#define PRESSURE_ADC       (&hadc1)

/* ---------------- Relay ---------------- */
/* Module relay kích mức cao: GPIO_PIN_SET. Module opto kích mức thấp: GPIO_PIN_RESET */
#define RELAY_ACTIVE_LEVEL     GPIO_PIN_SET

/* ---------------- MAX31865 / PT100 ---------------- */
#define MAX31865_RREF          430.0f   /* Ohm – điện trở tham chiếu trên module */
#define MAX31865_RNOMINAL      100.0f   /* PT100 */
#define MAX31865_WIRES         3        /* 2, 3 hoặc 4 dây */
#define MAX31865_FILTER_50HZ   1        /* 1 = lọc nhiễu lưới 50 Hz */
#define TEMP_OFFSET_C          0.0f     /* bù sai số nhiệt độ */

/* ---------------- Áp suất 0.5 – 4.5 V ---------------- */
#define PRESS_ADC_VREF         3.3f
#define PRESS_ADC_MAX          4095.0f
#define PRESS_DIVIDER_RATIO    1.5f     /* Vsensor = Vadc * ratio (R1=10k, R2=20k) */
#define PRESS_V_MIN            0.5f     /* V tại P_MIN */
#define PRESS_V_MAX            4.5f     /* V tại P_MAX */
#define PRESS_P_MIN_BAR        0.0f
#define PRESS_P_MAX_BAR        34.5f    /* 500 psi – chỉnh theo cảm biến thực tế */
#define PRESS_V_FAULT_LOW      0.25f    /* dưới mức này = đứt dây */
#define PRESS_V_FAULT_HIGH     4.80f    /* trên mức này = chập nguồn */

/* ---------------- Độ ẩm – RS485 Modbus RTU ---------------- */
/* Mặc định theo loại phổ biến XY-MD02 / SHT20 RS485: FC 0x04, reg 1 = T, reg 2 = RH, x10 */
#define HUM_MODBUS_ADDR        1
#define HUM_MODBUS_FUNC        0x04
#define HUM_REG_START          0x0001
#define HUM_REG_IDX_TEMP       0        /* vị trí thanh ghi nhiệt độ trong khung trả về */
#define HUM_REG_IDX_HUM        1        /* vị trí thanh ghi độ ẩm */
#define HUM_REG_COUNT          2
#define HUM_SCALE              10.0f
#define HUM_TIMEOUT_MS         200
#define HUM_MAX_ERRORS         3        /* lỗi liên tiếp trước khi báo mất cảm biến */

/* ---------------- Chu kỳ task (ms) ---------------- */
#define TASK_BUTTON_MS         10
#define TASK_SENSOR_MS         500
#define TASK_HUM_MS            1000
#define TASK_CTRL_MS           200
#define TASK_UI_MS             100
#define TASK_LOG_MS            2000
#define TASK_LED_MS            500

/* ---------------- Flash lưu thông số ---------------- */
#define SETTINGS_FLASH_ADDR    0x0800FC00UL   /* page 63 (1 KB) của 64 KB flash */

/* ---------------- Tiện ích ---------------- */
#define ARRAY_LEN(a)           (sizeof(a) / sizeof((a)[0]))

#endif /* APP_CONFIG_H */
