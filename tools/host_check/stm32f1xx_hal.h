/* Stub HAL tối giản để kiểm tra biên dịch App/ trên PC (KHÔNG dùng cho firmware, không thêm vào Keil) */
#ifndef STM32F1XX_HAL_STUB_H
#define STM32F1XX_HAL_STUB_H
#include <stdint.h>
#include <stdbool.h>
typedef struct { int dummy; } GPIO_TypeDef;
typedef struct { int dummy; } SPI_HandleTypeDef;
typedef struct { int dummy; } UART_HandleTypeDef;
typedef struct { int dummy; } ADC_HandleTypeDef;
typedef struct { int dummy; } I2C_HandleTypeDef;
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

HAL_StatusTypeDef HAL_I2C_Master_Transmit(I2C_HandleTypeDef*, uint16_t, uint8_t*, uint16_t, uint32_t);
HAL_StatusTypeDef HAL_I2C_Master_Receive(I2C_HandleTypeDef*, uint16_t, uint8_t*, uint16_t, uint32_t);
HAL_StatusTypeDef HAL_I2C_Init(I2C_HandleTypeDef*);
HAL_StatusTypeDef HAL_I2C_DeInit(I2C_HandleTypeDef*);
typedef struct { uint32_t Pin, Mode, Pull, Speed; } GPIO_InitTypeDef;
void HAL_GPIO_Init(GPIO_TypeDef*, GPIO_InitTypeDef*);
#define GPIO_MODE_OUTPUT_OD 0x11
#define GPIO_NOPULL 0
#define GPIO_SPEED_FREQ_HIGH 3
#define FLASH_PAGE_SIZE 0x400U
#define __HAL_RCC_I2C2_FORCE_RESET()   ((void)0)
#define __HAL_RCC_I2C2_RELEASE_RESET() ((void)0)
#define GPIO_PIN_10 0x0400
#define GPIO_PIN_11 0x0800
#define GPIO_MODE_AF_OD 0x12
#endif
