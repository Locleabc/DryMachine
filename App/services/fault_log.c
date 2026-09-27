/**
 * @file    fault_log.c
 * @note    Flash STM32F1 chịu ~10 000 lần ghi/page: chỉ ghi khi có lỗi mới hoặc khi xoá lịch sử.
 */
#include "fault_log.h"
#include <stddef.h>
#include <string.h>

#define LOG_MAGIC   0x464C4F47UL    /* "FLOG" */

typedef struct {
    uint32_t          magic;
    uint8_t           count;         /* số mục hợp lệ */
    uint8_t           head;          /* vị trí sẽ ghi mục tiếp theo */
    uint16_t          reserved;
    fault_log_entry_t e[FAULT_LOG_MAX];
    uint32_t          crc;
} log_blob_t;

static log_blob_t s_log;
static const flash_store_cfg_t *s_store;

static uint32_t crc32_calc(const uint8_t *p, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFFUL;
    while (len--) {
        crc ^= *p++;
        for (int i = 0; i < 8; i++) crc = (crc & 1) ? (crc >> 1) ^ 0xEDB88320UL : (crc >> 1);
    }
    return ~crc;
}

static uint32_t calc_crc(const log_blob_t *b)
{
    return crc32_calc((const uint8_t *)b, offsetof(log_blob_t, crc));
}

static bool save(void)
{
    s_log.magic = LOG_MAGIC;
    s_log.crc = calc_crc(&s_log);
    return s_store && FlashStore_Write(s_store, &s_log, sizeof(s_log));
}

void FaultLog_Init(const flash_store_cfg_t *store)
{
    log_blob_t tmp;
    s_store = store;
    if (store && FlashStore_Read(store, &tmp, sizeof(tmp)) && tmp.magic == LOG_MAGIC &&
        tmp.crc == calc_crc(&tmp) && tmp.count <= FAULT_LOG_MAX && tmp.head < FAULT_LOG_MAX) {
        s_log = tmp;
    } else {
        memset(&s_log, 0, sizeof(s_log));
    }
}

bool FaultLog_Add(uint32_t time, uint16_t code)
{
    s_log.e[s_log.head].time = time;
    s_log.e[s_log.head].code = code;
    s_log.e[s_log.head].reserved = 0;
    s_log.head = (uint8_t)((s_log.head + 1) % FAULT_LOG_MAX);
    if (s_log.count < FAULT_LOG_MAX) s_log.count++;
    return save();
}

uint8_t FaultLog_Count(void)
{
    return s_log.count;
}

bool FaultLog_Get(uint8_t idx, fault_log_entry_t *out)
{
    if (idx >= s_log.count) return false;
    uint8_t pos = (uint8_t)((s_log.head + FAULT_LOG_MAX - 1 - idx) % FAULT_LOG_MAX);
    *out = s_log.e[pos];
    return true;
}

bool FaultLog_Clear(void)
{
    memset(&s_log, 0, sizeof(s_log));
    return save();
}
