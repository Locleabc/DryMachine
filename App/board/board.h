/**
 * @file    board.h
 * @brief   Mô tả phần cứng board: gán chân, handle HAL, hằng số hiệu chuẩn.
 *          Đây là nơi DUY NHẤT biết tên chân CubeMX (main.h). Đổi phần cứng → sửa board.c.
 */
#ifndef BOARD_H
#define BOARD_H

#include "relay.h"
#include "button.h"
#include "sht4x.h"
#include "ntc.h"
#include "ili9341.h"
#include "flash_store.h"

/* Chỉ số relay trong board_relays[] */
enum {
    RLY_ID_COMP = 0,        /* máy nén (qua contactor) */
    RLY_ID_FAN_EVAP,        /* quạt dàn lạnh */
    RLY_ID_FAN_S1,          /* quạt dàn nóng cấp 1..5 – cộng dồn: cấp N đóng relay S1..SN */
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
extern const sht4x_cfg_t        board_sht45;
extern const ntc_cfg_t          board_ntc_chamber; /* NTC nhiệt độ buồng – PA0 */
extern const ntc_cfg_t          board_ntc_evap;    /* NTC dàn lạnh – PA6 */
extern const ili9341_cfg_t      board_lcd;
extern const flash_store_cfg_t  board_settings_flash;
extern const flash_store_cfg_t  board_faultlog_flash;

uint32_t Board_Millis(void);
void     Board_LedToggle(void);
void     Board_LcdBacklight(bool on);   /* đèn nền TFT (chân LED của module) – PA11 */
void     Board_LogWrite(const char *data, uint16_t len);
void     Board_I2cRecover(void);        /* gỡ treo bus I2C của SHT45 */

/* Quét bus I2C2 bằng GPIO (bit-bang, ~10 kHz), dùng cho chẩn đoán đấu dây cảm biến.
 * Quét 2 lần: đúng chiều (SCL=PB10, SDA=PB11) và ĐẢO chiều (SCL=PB11, SDA=PB10).
 * Xong tự gọi Board_I2cRecover() để trả chân về I2C phần cứng. */
#define BOARD_I2C_SCAN_MAX 4
typedef struct {
    uint8_t scl_high, sda_high;                     /* mức chân lúc rảnh: 1 = cao (có pull-up) */
    uint8_t n_found;   uint8_t found[BOARD_I2C_SCAN_MAX];     /* địa chỉ trả ACK, đúng chiều */
    uint8_t n_swapped; uint8_t swapped[BOARD_I2C_SCAN_MAX];   /* địa chỉ trả ACK khi đảo SCL/SDA */
} board_i2c_scan_t;
void     Board_I2cScan(board_i2c_scan_t *r);


#endif
