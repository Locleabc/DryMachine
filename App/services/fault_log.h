/**
 * @file    fault_log.h
 * @brief   Lịch sử lỗi: vòng đệm FAULT_LOG_MAX mục, lưu Flash (qua flash_store) mỗi khi có lỗi mới.
 *          Không biết mã lỗi nghĩa là gì – chỉ lưu (thời điểm, mã).
 */
#ifndef FAULT_LOG_H
#define FAULT_LOG_H

#include "flash_store.h"
#include <stdbool.h>
#include <stdint.h>

#define FAULT_LOG_MAX   16

typedef struct {
    uint32_t time;          /* giây từ 01/01/2000 (0 = chưa có giờ) */
    uint16_t code;          /* mã lỗi (1 bit DRYER_FAULT_*) */
    uint16_t reserved;
} fault_log_entry_t;

void    FaultLog_Init(const flash_store_cfg_t *store);
bool    FaultLog_Add(uint32_t time, uint16_t code);                /* ghi Flash ngay */
uint8_t FaultLog_Count(void);
bool    FaultLog_Get(uint8_t idx, fault_log_entry_t *out);         /* 0 = mới nhất */
bool    FaultLog_Clear(void);

#endif
