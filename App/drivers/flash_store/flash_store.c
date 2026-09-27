/**
 * @file    flash_store.c
 */
#include "flash_store.h"
#include <string.h>

#ifndef FLASH_PAGE_SIZE
#define FLASH_PAGE_SIZE 0x400U
#endif

bool FlashStore_Read(const flash_store_cfg_t *cfg, void *dst, uint32_t len)
{
    if (len > cfg->size) return false;
    memcpy(dst, (const void *)(uintptr_t)cfg->addr, len);
    return true;
}

bool FlashStore_Write(const flash_store_cfg_t *cfg, const void *src, uint32_t len)
{
    if (len > cfg->size) return false;

    FLASH_EraseInitTypeDef er = {0};
    uint32_t page_err = 0;
    er.TypeErase   = FLASH_TYPEERASE_PAGES;
    er.PageAddress = cfg->addr;
    er.NbPages     = (cfg->size + FLASH_PAGE_SIZE - 1) / FLASH_PAGE_SIZE;

    bool ok = true;
    HAL_FLASH_Unlock();
    if (HAL_FLASHEx_Erase(&er, &page_err) != HAL_OK) ok = false;

    const uint8_t *p = (const uint8_t *)src;
    for (uint32_t i = 0; ok && i < len; i += 2) {
        uint16_t hw = p[i];
        if (i + 1 < len) hw |= (uint16_t)(p[i + 1] << 8);
        else             hw |= 0xFF00;
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, cfg->addr + i, hw) != HAL_OK) ok = false;
    }
    HAL_FLASH_Lock();

    return ok && memcmp((const void *)(uintptr_t)cfg->addr, src, len) == 0;
}
