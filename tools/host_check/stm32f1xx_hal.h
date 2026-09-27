/* Stub HAL tối giản để kiểm tra biên dịch App/ trên PC (không dùng cho firmware) */
#ifndef MAIN_H_STUB
#define MAIN_H_STUB
#include <stdint.h>
#include <stdbool.h>
typedef struct { int dummy; } GPIO_TypeDef;
typedef struct { int dummy; } SPI_HandleTypeDef;
typedef struct { int dummy; } UART_HandleTypeDef;
typedef struct { int dummy; } ADC_HandleTypeDef;
typedef enum { GPIO_PIN_RESET = 0, GPIO_PIN_SET } GPIO_PinState;
typedef enum { HAL_OK = 0, HAL_ERROR, HAL_BUSY, HAL_TIMEOUT } HAL_StatusTypeDef;
typedef struct { uint32_t TypeErase, Banks, PageAddress, NbPages; } FLASH_EraseInitTypeDef;
#define FLASH_TYPEERASE_PAGES 0
#define FLASH_TYPEPROGRAM_HALFWORD 1
extern GPIO_TypeDef GPIOA_s, GPIOB_s, GPIOC_s;
#define GPIOA (&GPIOA_s)
#define GPIOB (&GPIOB_s)
#define GPIOC (&GPIOC_s)
void HAL_GPIO_WritePin(GPIO_TypeDef*, uint16_t, GPIO_PinState);
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef*, uint16_t);
void HAL_GPIO_TogglePin(GPIO_TypeDef*, uint16_t);
uint32_t HAL_GetTick(void);
void HAL_Delay(uint32_t);
HAL_StatusTypeDef HAL_SPI_Transmit(SPI_HandleTypeDef*, uint8_t*, uint16_t, uint32_t);
HAL_StatusTypeDef HAL_SPI_Receive(SPI_HandleTypeDef*, uint8_t*, uint16_t, uint32_t);
HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef*, const uint8_t*, uint16_t, uint32_t);
HAL_StatusTypeDef HAL_UART_Receive_IT(UART_HandleTypeDef*, uint8_t*, uint16_t);
HAL_StatusTypeDef HAL_UART_AbortReceive(UART_HandleTypeDef*);
#define __HAL_UART_CLEAR_OREFLAG(h) ((void)(h))
HAL_StatusTypeDef HAL_ADC_Start(ADC_HandleTypeDef*);
HAL_StatusTypeDef HAL_ADC_Stop(ADC_HandleTypeDef*);
HAL_StatusTypeDef HAL_ADC_PollForConversion(ADC_HandleTypeDef*, uint32_t);
uint32_t HAL_ADC_GetValue(ADC_HandleTypeDef*);
HAL_StatusTypeDef HAL_ADCEx_Calibration_Start(ADC_HandleTypeDef*);
HAL_StatusTypeDef HAL_FLASH_Unlock(void);
HAL_StatusTypeDef HAL_FLASH_Lock(void);
HAL_StatusTypeDef HAL_FLASHEx_Erase(FLASH_EraseInitTypeDef*, uint32_t*);
HAL_StatusTypeDef HAL_FLASH_Program(uint32_t, uint32_t, uint64_t);

/* User Label – giống CubeMX sinh ra theo docs/PINOUT.md */
#define PRESS_ADC_Pin 0x0001
#define PRESS_ADC_GPIO_Port GPIOA
#define RS485_DE_Pin 0x0002
#define RS485_DE_GPIO_Port GPIOA
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
#define RLY_FAN_COND_Pin 0x0040
#define RLY_FAN_COND_GPIO_Port GPIOB
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
