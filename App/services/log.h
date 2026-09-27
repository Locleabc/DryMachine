/**
 * @file    log.h
 * @brief   Log dạng printf. Không biết UART nào – hàm ghi được truyền vào lúc Init.
 */
#ifndef LOG_H
#define LOG_H
#include <stdint.h>

typedef void (*log_write_fn)(const char *data, uint16_t len);

void Log_Init(log_write_fn write);
void Log_Printf(const char *fmt, ...);     /* tự thêm \r\n; không hỗ trợ %f – dùng Fmt_Float */

#endif
