/**
 * @file    ui_menu.h
 * @brief   Giao diện TFT: màn hình chính, menu thông số, chỉnh giá trị.
 *
 *  Màn hình chính:  ENTER (nhấn)  -> Menu cài đặt
 *                   ENTER (giữ 1s) -> Chạy / Dừng
 *                   EXIT  (giữ 1s) -> Reset lỗi
 *  Menu:            UP/DOWN chọn, ENTER sửa, EXIT về (tự lưu nếu có thay đổi)
 *  Sửa giá trị:     UP/DOWN tăng/giảm (giữ để lặp), ENTER xác nhận, EXIT huỷ
 */
#ifndef UI_MENU_H
#define UI_MENU_H

#include "app.h"
#include "bsp_button.h"
#include "ctrl_dryer.h"

void UI_Init(void);
void UI_HandleButton(const btn_event_t *evt);
void UI_Update(const app_meas_t *m, const ctrl_status_t *st);
void UI_Message(const char *msg);   /* hiện thông báo ngắn ở dòng trạng thái */

#endif
