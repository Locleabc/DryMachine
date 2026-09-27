# DryMachine – Bộ điều khiển máy sấy tách ẩm (bơm nhiệt từ điều hoà)

MCU: **STM32F103C8T6** (72 MHz, 64 KB Flash) – IDE: **Keil MDK-ARM (AC6)** + STM32CubeMX (HAL).

## Phần cứng

| # | Chức năng | Linh kiện / Giao tiếp |
|---|-----------|-----------------------|
| 1 | Nhiệt độ buồng sấy | PT100 + MAX31865 (SPI2) |
| 2 | Độ ẩm (+ nhiệt độ phụ) | **SHT45** (I2C2) |
| 3 | Quạt dàn nóng | Relay trung gian |
| 4 | Máy nén | Relay trung gian (khuyến nghị qua contactor) |
| 5 | Áp suất gas | Cảm biến 0.5–4.5 V → phân áp → ADC1_IN0 |
| 6 | Quạt dàn lạnh | Relay trung gian |
| 7 | Màn hình | TFT 3.2" ILI9341 320x240 (SPI1) |
| 8 | Nút bấm | UP / DOWN / ENTER / EXIT |

* Sơ đồ chân: [docs/PINOUT.md](docs/PINOUT.md)
* Tạo project Keil: [docs/KEIL_SETUP.md](docs/KEIL_SETUP.md)
* Kiến trúc & quy tắc phụ thuộc: [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)

## Cấu trúc thư mục

```
App/
 ├─ app.c/.h              Keo nối: nơi DUY NHẤT biết tất cả module
 ├─ board/                Gán chân, handle HAL, bảng phần cứng (chỉ file này include main.h)
 ├─ drivers/              Driver thiết bị – mỗi driver 1 thư mục, chỉ phụ thuộc HAL
 │   ├─ button/  relay/  max31865/  sht4x/  press_analog/  ili9341/  flash_store/
 ├─ services/             sensors (đọc + lọc), settings (thông số + lưu), log, sched, util_fmt
 ├─ control/              dryer_ctrl – logic điều khiển THUẦN C, không HAL (test được trên PC)
 └─ ui/                   Giao diện TFT – chỉ nhận ui_view_t, trả lệnh qua callback
Core/, Drivers/, MDK-ARM/ Do CubeMX sinh ra
tools/host_check/         Biên dịch thử + test logic trên PC
```

## Lịch sử
`git log --oneline` – mỗi bước phát triển là 1 commit.
