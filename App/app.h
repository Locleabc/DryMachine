/**
 * @file    app.h
 * @brief   Điểm vào ứng dụng. main.c: App_Init() sau MX_xxx_Init(), App_Loop() trong while(1).
 */
#ifndef APP_H
#define APP_H

/* 1 = chạy chương trình THỬ cảm biến độ ẩm (App/hum_monitor.c) thay bộ điều khiển máy sấy.
 *     Relay luôn nhả. Xem g_hum trong Keil Watch. Đặt 0 để về firmware máy sấy bình thường. */
#ifndef APP_HUM_MONITOR
#define APP_HUM_MONITOR  1
#endif

void App_Init(void);
void App_Loop(void);

#endif
