/**
 * @file    sched.c
 */
#include "sched.h"

void Sched_Init(sched_task_t *t, uint8_t n, uint32_t now)
{
    for (uint8_t i = 0; i < n; i++) t[i].last = now;
}

void Sched_Run(sched_task_t *t, uint8_t n, uint32_t now)
{
    for (uint8_t i = 0; i < n; i++) {
        if (t[i].fn && (now - t[i].last) >= t[i].period_ms) {
            t[i].last = now;
            t[i].fn(now);
        }
    }
}
