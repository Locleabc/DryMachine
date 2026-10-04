/**
 * @file    board.h
 * @brief   Mô tả phần cứng board: gán chân, handle HAL, hằng số hiệu chuẩn.
 *          Đây là nơi DUY NHẤT biết tên chân CubeMX (main.h). Đổi phần cứng → sửa board.c.
 */
#ifndef BOARD_H
#define BOARD_H

#include "relay.h"
#include "button.h"
#include "max31865.h"
#include "sht4x.h"
#include "press_analog.h"
#include "ili9341.h"
#include "flash_store.h"

/* Chỉ số relay trong board_relays[] */
enum {
    RLY_ID_COMP = 0,        /* máy nén (qua contactor) */
    RLY_ID_FAN_EVAP,        /* quạt dàn lạnh */
    RLY_ID_FAN_S1,          /* quạt dàn nóng cấp 1..5 – chỉ 1 relay đóng tại một thời điểm */
    RLY_ID_FAN_S2,
    RLY_ID_FAN_S3,
    RLY_ID_FAN_S4,
    RLY_ID_FAN_S5,
    RLY_ID_COUNT
};

/* Chỉ số nút trong board_buttons[] */
enum { BTN_ID_UP = 0, BTN_ID_DOWN, BTN_ID_ENTER, BTN_ID_EXIT, BTN_ID_COUNT };

extern const relay_hw_t         board_relays[RLY_ID_COUNT];
extern const button_hw_t        board_buttons[BTN_ID_COUNT];
extern const max31865_cfg_t     board_pt100;
extern const sht4x_cfg_t        board_sht45;
extern const press_analog_cfg_t board_press;
extern const ili9341_cfg_t      board_lcd;
extern const flash_store_cfg_t  board_settings_flash;
extern const flash_store_cfg_t  board_faultlog_flash;

uint32_t Board_Millis(void);
void     Board_LedToggle(void);
void     Board_LcdBacklight(bool on);   /* đèn nền TFT (chân LED của module) – PA11 */
void     Board_LogWrite(const char *data, uint16_t len);
void     Board_I2cRecover(void);        /* gỡ treo bus I2C của SHT45 */

/* Đồng hồ thời gian thực (RTC nội, thạch anh 32.768 kHz, pin CR2032 ở VBAT) */
void     Board_RtcInit(void);
bool     Board_RtcRead(uint32_t *seconds);  /* false nếu chưa từng chỉnh giờ */
void     Board_RtcWrite(uint32_t seconds);

#endif
