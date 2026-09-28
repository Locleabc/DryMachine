/* Stub main.h – mô phỏng file CubeMX sinh ra (User Label theo docs/PINOUT.md) */
#ifndef MAIN_H_STUB
#define MAIN_H_STUB
#include "stm32f1xx_hal.h"
extern SPI_HandleTypeDef  hspi1, hspi2;
extern UART_HandleTypeDef huart1;
extern ADC_HandleTypeDef  hadc1;
extern I2C_HandleTypeDef  hi2c2;
/* User Label – giống CubeMX sinh ra theo docs/PINOUT.md */
#define PRESS_ADC_Pin 0x0001
#define PRESS_ADC_GPIO_Port GPIOA
#define TFT_CS_Pin 0x0010
#define TFT_CS_GPIO_Port GPIOA
#define BTN_ENTER_Pin 0x0100
#define BTN_ENTER_GPIO_Port GPIOA
#define BTN_EXIT_Pin 0x8000
#define BTN_EXIT_GPIO_Port GPIOA
#define TFT_DC_Pin 0x0001
#define TFT_DC_GPIO_Port GPIOB
#define TFT_RST_Pin 0x0002
#define TFT_RST_GPIO_Port GPIOB
#define RLY_COMP_Pin 0x0020
#define RLY_COMP_GPIO_Port GPIOB
#define RLY_FAN_S1_Pin 0x0040
#define RLY_FAN_S1_GPIO_Port GPIOB
#define RLY_FAN_S2_Pin 0x0002
#define RLY_FAN_S2_GPIO_Port GPIOA
#define RLY_FAN_S3_Pin 0x0004
#define RLY_FAN_S3_GPIO_Port GPIOA
#define RLY_FAN_S4_Pin 0x0008
#define RLY_FAN_S4_GPIO_Port GPIOA
#define RLY_FAN_S5_Pin 0x0010
#define RLY_FAN_S5_GPIO_Port GPIOB
#define RLY_FAN_EVAP_Pin 0x0080
#define RLY_FAN_EVAP_GPIO_Port GPIOB
#define BTN_UP_Pin 0x0100
#define BTN_UP_GPIO_Port GPIOB
#define BTN_DOWN_Pin 0x0200
#define BTN_DOWN_GPIO_Port GPIOB
#define MAX_CS_Pin 0x1000
#define MAX_CS_GPIO_Port GPIOB
#define LED_RUN_Pin 0x2000
#define LED_RUN_GPIO_Port GPIOC
#endif
