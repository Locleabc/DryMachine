/**
 * @file    sched.h
 * @brief   Bộ lập lịch cooperative tối giản (không RTOS).
 */
#ifndef SCHED_H
#define SCHED_H
#include <stdint.h>

typedef struct {
    uint32_t period_ms;
    void   (*fn)(uint32_t now_ms);
    uint32_t last;
} sched_task_t;

void Sched_Init(sched_task_t *tasks, uint8_t count, uint32_t now_ms);
void Sched_Run(sched_task_t *tasks, uint8_t count, uint32_t now_ms);

#endif
