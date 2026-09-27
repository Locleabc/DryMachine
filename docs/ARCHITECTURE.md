# Kiến trúc phần mềm

```
                 ┌──────────────┐
                 │    app.c     │  keo nối (glue) – biết tất cả, không chứa logic
                 └──────┬───────┘
        ┌───────────────┼──────────────────┬──────────────┐
        ▼               ▼                  ▼              ▼
   ┌─────────┐   ┌─────────────┐    ┌────────────┐  ┌──────────┐
   │   ui    │   │  services   │    │  control   │  │  board   │
   │(ui_view)│   │sensors/sett.│    │ dryer_ctrl │  │ pin/HAL  │
   └────┬────┘   └──────┬──────┘    └────────────┘  └────┬─────┘
        ▼               ▼             (thuần C)          ▼
   ┌─────────────────────────────────────────────────────────┐
   │ drivers: ili9341 · max31865 · sht4x · press_analog ·    │
   │          button · relay · flash_store                   │
   └──────────────────────────┬──────────────────────────────┘
                              ▼
                        STM32 HAL (CubeMX)
```

## Quy tắc phụ thuộc (bắt buộc giữ)

| Tầng | Được include | KHÔNG được include |
|------|--------------|--------------------|
| `drivers/*` | `stm32f1xx_hal.h`, header của chính nó | `main.h`, driver khác, services, ui, control |
| `control/` | `<stdint.h>`, `<stdbool.h>` | HAL, drivers, services, ui |
| `services/` | drivers cần dùng, `control/dryer_ctrl.h` (kiểu dữ liệu thông số) | ui, board, main.h |
| `ui/` | `ili9341.h`, `util_fmt.h` | control, services, board |
| `board/` | `main.h`, header driver (để khai báo cấu hình) | services, ui, control |
| `app.c` | tất cả | – |

Hệ quả:
* **Đổi phần cứng** (chân, cảm biến khác loại) → chỉ sửa `board/` và driver tương ứng.
* **Đổi logic điều khiển** → chỉ sửa `control/`, test trên PC bằng `tools/host_check`.
* **Đổi giao diện** → chỉ sửa `ui/`. UI không gọi thẳng điều khiển: nó trả lệnh
  (`UI_CMD_START_STOP`, `UI_CMD_RESET_FAULT`, `UI_CMD_SAVE_SETTINGS`) qua callback cho `app.c`.
* Driver nhận **struct cấu hình** (handle SPI/I2C/ADC, chân CS…) lúc Init, không tự biết mình gắn ở đâu.

## Luồng dữ liệu mỗi vòng

1. `Sensors_Process()` đọc PT100 / áp suất (500 ms) và SHT45 (1 s, không chặn) → `sensors_data_t`
2. `app.c` chép dữ liệu cảm biến → `dryer_input_t` → `DryerCtrl_Step()` → `dryer_output_t` → `Relay_Set()`
3. `app.c` gộp cảm biến + trạng thái điều khiển + thông số → `ui_view_t` → `UI_Update()`
4. Nút bấm → `Button_GetEvent()` → `app.c` đổi sang `ui_key_t` → `UI_Key()` → callback lệnh

## Thêm 1 thông số cài đặt

1. Thêm field vào `dryer_params_t` (hoặc `settings_t`)
2. Thêm 1 dòng vào bảng `s_params[]` trong `services/settings.c`, giá trị mặc định trong `Settings_Default()`
3. Tăng `SETTINGS_VERSION` (Flash cũ tự bị bỏ, nạp mặc định)

## Giao diện (ui/)

Mỗi màn hình là một `ui_screen_t` trong một file riêng, `ui_core.c` lo header/footer/chuyển màn hình:

| File | Màn hình |
|------|----------|
| `ui_page_main.c` | Trang 1 – nhiệt độ, độ ẩm, điểm đặt, chế độ, trạng thái |
| `ui_page_run.c` | Trang 2 – chạy / dừng |
| `ui_page_timer.c` | Trang 3 – thời gian sấy |
| `ui_page_faults.c` | Trang 4 – lỗi hiện tại + lịch sử |
| `ui_menu_preset.c` | Menu chế độ sấy (giữ ENTER 3 s) + chỉnh đồng hồ |
| `ui_edit.c` | Nhập số từng chữ số (dùng chung) |
| `ui_menu_tech.c` | Menu kỹ thuật ẩn (giữ EXIT 3 s ở trang chính) |

Thêm trang mới: tạo file `ui_page_xxx.c` với một `ui_screen_t`, thêm vào `ui_scr_t` và bảng `s_screens[]`.
Ảnh chụp từ bộ mô phỏng: [UI.md](UI.md).
