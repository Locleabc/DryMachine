# Tạo project Keil MDK-ARM

## 1. Sinh code bằng STM32CubeMX

1. New Project → **STM32F103C8Tx** → lưu file `DryMachine.ioc` vào **thư mục gốc repo**.
2. Cấu hình:
   * **SYS**: Debug = *Serial Wire*, Timebase = SysTick
   * **RCC**: HSE = Crystal/Ceramic Resonator, **LSE = Crystal/Ceramic Resonator** (thạch anh 32.768 kHz trên Blue Pill)
   * **RTC**: tick *Activate Clock Source*, **không** tick *Activate Calendar* (firmware tự đọc bộ đếm RTC);
     Clock Configuration → RTC Clock Mux = **LSE**
   * **Clock**: HSE 8 MHz → PLL x9 → SYSCLK 72 MHz, APB1 36 MHz, APB2 72 MHz, ADC /6 (12 MHz)
   * **SPI1** (TFT): Transmit Only Master (hoặc Full-Duplex), 8 bit, CPOL Low, CPHA 1 Edge, NSS Software, Prescaler 4
   * **SPI2** (MAX31865): Full-Duplex Master, 8 bit, **CPOL Low, CPHA 2 Edge**, NSS Software, Prescaler 16
   * **I2C2** (SHT45): Standard Mode 100 kHz
   * **USART1**: Asynchronous 115200 8N1 (log)
   * **ADC1**: IN0, Continuous Disabled, Sampling 239.5 cycles
   * **GPIO**: theo [PINOUT.md](PINOUT.md), đặt đúng **User Label**
3. Project Manager:
   * Project Name `DryMachine`, Project Location = thư mục **cha** của repo (để CubeMX dùng đúng thư mục repo)
   * **Toolchain / IDE = MDK-ARM**, Min Version V5.32
   * Code Generator: tick *Generate peripheral initialization as a pair of '.c/.h' files*
4. GENERATE CODE → có thư mục `MDK-ARM/DryMachine.uvprojx`, `Core/`, `Drivers/`.

## 2. Thêm code ứng dụng vào Keil

Mở `MDK-ARM/DryMachine.uvprojx`.

**Groups** (chuột phải Target → Manage Project Items → Groups): tạo các nhóm và *Add Files*:

| Group | File |
|-------|------|
| App/app | `App/app.c`, `App/board/board.c` |
| App/drivers | toàn bộ `App/drivers/*/*.c` |
| App/services | `App/services/*.c` |
| App/control | `App/control/*.c` |
| App/ui | `App/ui/*.c` |

**Include Paths** (Options for Target → C/C++ (AC6) → Include Paths) – dán nguyên dòng:

```
../App;../App/board;../App/control;../App/services;../App/ui;../App/drivers/button;../App/drivers/relay;../App/drivers/max31865;../App/drivers/sht4x;../App/drivers/press_analog;../App/drivers/ili9341;../App/drivers/flash_store
```

**Options for Target**:
* *Target*: ARM Compiler = **Use default compiler version 6** (AC6); tick **Use MicroLIB**
* *Target*: **IROM1 Size = 0xF800** (62 KB) – 2 page cuối dành cho lịch sử lỗi (0x0800F800) và thông số (0x0800FC00)
* *C/C++ (AC6)*: Language C = **c11** (hoặc gnu11), Optimization **-Os balanced** nếu thiếu Flash
* *Debug*: ST-Link Debugger → Settings → Port **SW**; *Utilities*: tick *Use Debug Driver*

**Encoding**: Edit → Configuration → Editor → Encoding = **UTF-8 without signature**
(comment tiếng Việt trong code là UTF-8).

## 3. Sửa `Core/Src/main.c` – chỉ trong vùng USER CODE

```c
/* USER CODE BEGIN Includes */
#include "app.h"
/* USER CODE END Includes */

  /* USER CODE BEGIN 2 */
  App_Init();
  /* USER CODE END 2 */

  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    App_Loop();
  }
  /* USER CODE END 3 */
```

Build (F7) → Download (F8).

> Sinh lại code từ CubeMX không làm mất Groups/Include Paths đã thêm trong Keil
> và không mất code trong vùng USER CODE.

## 4. Kiểm tra nhanh trên PC (không cần board)

```sh
sh tools/host_check/check.sh      # biên dịch thử toàn bộ App/ với HAL giả lập + chạy test logic điều khiển
```
