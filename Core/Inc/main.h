/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f1xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define LED_RUN_Pin GPIO_PIN_13
#define LED_RUN_GPIO_Port GPIOC
#define PRESS_ADC_Pin GPIO_PIN_0
#define PRESS_ADC_GPIO_Port GPIOA
#define RLY_FAN_S2_Pin GPIO_PIN_1
#define RLY_FAN_S2_GPIO_Port GPIOA
#define RLY_FAN_S3_Pin GPIO_PIN_2
#define RLY_FAN_S3_GPIO_Port GPIOA
#define RLY_FAN_S4_Pin GPIO_PIN_3
#define RLY_FAN_S4_GPIO_Port GPIOA
#define TFT_CS_Pin GPIO_PIN_4
#define TFT_CS_GPIO_Port GPIOA
#define TFT_DC_Pin GPIO_PIN_0
#define TFT_DC_GPIO_Port GPIOB
#define TFT_RST_Pin GPIO_PIN_1
#define TFT_RST_GPIO_Port GPIOB
#define MAX_CS_Pin GPIO_PIN_12
#define MAX_CS_GPIO_Port GPIOB
#define BTN_ENTER_Pin GPIO_PIN_8
#define BTN_ENTER_GPIO_Port GPIOA
#define BTN_EXIT_Pin GPIO_PIN_15
#define BTN_EXIT_GPIO_Port GPIOA
#define RLY_COMP_Pin GPIO_PIN_5
#define RLY_COMP_GPIO_Port GPIOB
#define RLY_FAN_S1_Pin GPIO_PIN_6
#define RLY_FAN_S1_GPIO_Port GPIOB
#define RLY_FAN_EVAP_Pin GPIO_PIN_7
#define RLY_FAN_EVAP_GPIO_Port GPIOB
#define RLY_FAN_S5_Pin GPIO_PIN_4
#define RLY_FAN_S5_GPIO_Port GPIOB
#define BTN_UP_Pin GPIO_PIN_8
#define BTN_UP_GPIO_Port GPIOB
#define BTN_DOWN_Pin GPIO_PIN_9
#define BTN_DOWN_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
