/**
 * @file    flash_store.h
 * @brief   Lưu 1 khối dữ liệu nhỏ vào 1 page Flash nội (STM32F1). Không biết nội dung.
 *          Nhớ giảm vùng code (Keil IROM1) để không đè lên page này.
 */
#ifndef FLASH_STORE_H
#define FLASH_STORE_H

#include "stm32f1xx_hal.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint32_t addr;          /* địa chỉ đầu page, vd 0x0800FC00 */
    uint32_t size;          /* kích thước vùng (bội số page) */
} flash_store_cfg_t;

bool FlashStore_Read(const flash_store_cfg_t *cfg, void *dst, uint32_t len);
bool FlashStore_Write(const flash_store_cfg_t *cfg, const void *src, uint32_t len);

#endif
