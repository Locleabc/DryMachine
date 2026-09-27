# DryMachine – Bộ điều khiển máy sấy tách ẩm (bơm nhiệt từ điều hoà)

MCU: **STM32F103C8T6** (72 MHz, 64 KB Flash) – IDE: **STM32CubeIDE** (HAL).

## Phần cứng

| # | Chức năng | Linh kiện / Giao tiếp |
|---|-----------|-----------------------|
| 1 | Nhiệt độ buồng sấy | PT100 + MAX31865 (SPI2) |
| 2 | Độ ẩm | Cảm biến RS485 Modbus RTU (USART2 + MAX485) – *chưa chốt mã* |
| 3 | Quạt dàn nóng | Relay trung gian |
| 4 | Máy nén | Relay trung gian (khuyến nghị qua contactor) |
| 5 | Áp suất gas | Cảm biến 0.5–4.5 V → phân áp → ADC1_IN0 |
| 6 | Quạt dàn lạnh | Relay trung gian |
| 7 | Màn hình | TFT 3.2" ILI9341 320x240 (SPI1) |
| 8 | Nút bấm | UP / DOWN / ENTER / EXIT |

Chi tiết chân: [docs/PINOUT.md](docs/PINOUT.md) – Cấu hình CubeMX: [docs/CUBEMX_SETUP.md](docs/CUBEMX_SETUP.md)

## Cấu trúc mã nguồn

```
App/
 ├─ Inc/ , Src/
 │   app_config.h      Cấu hình chung: handle ngoại vi, hằng số hiệu chuẩn, chu kỳ task
 │   app.c/.h          App_Init() / App_Loop() – bộ lập lịch đơn giản theo HAL_GetTick()
 │   bsp_relay.c/.h    Điều khiển 3 relay
 │   bsp_button.c/.h   Quét 4 nút: chống dội, nhấn ngắn, nhấn giữ, lặp
 │   drv_max31865.c/.h PT100 qua MAX31865
 │   drv_pressure.c/.h Áp suất analog 0.5–4.5 V
 │   drv_humidity.c/.h Độ ẩm RS485 Modbus RTU (non-blocking)
 │   drv_ili9341.c/.h  Driver TFT + font 5x7
 │   settings.c/.h     Bảng thông số, lưu Flash (page cuối) có CRC
 │   ctrl_dryer.c/.h   Máy trạng thái điều khiển + bảo vệ máy nén
 │   ui_menu.c/.h      Màn hình chính / menu / chỉnh thông số
Core/                  Do CubeMX sinh ra (main.c, HAL init …)
docs/                  Tài liệu
```

Nguyên tắc: **mọi code ứng dụng nằm trong `App/`**, `Core/` chỉ thêm 3 dòng trong vùng
`USER CODE` để CubeMX sinh lại code không bị mất.

## Lịch sử
Xem `git log --oneline`. Mỗi bước phát triển là 1 commit.
