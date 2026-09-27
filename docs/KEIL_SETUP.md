# Tạo project Keil MDK-ARM

Cần cài: **STM32CubeMX** (6.x), **Keil MDK 5** (Arm Compiler 6) và pack **Keil.STM32F1xx_DFP**
(Keil → Pack Installer → STMicroelectronics → STM32F1 Series → Install).

## Cách nhanh (3 bước)

1. **Mở `DryMachine.ioc`** (thư mục gốc repo) bằng STM32CubeMX.
   File này đã cấu hình sẵn toàn bộ: chân + User Label, clock 72 MHz, SPI1, SPI2, I2C2, USART1, ADC1,
   **RTC dùng LSE (chỉ Activate Clock Source, không Calendar)**, SWD, Toolchain = MDK-ARM.
   Nếu CubeMX hỏi *migrate* sang bản firmware mới hơn → chọn **Migrate**.
2. Bấm **GENERATE CODE** (góc trên bên phải). CubeMX tạo `Core/`, `Drivers/`, `MDK-ARM/`.
3. Chạy script gắn code ứng dụng vào Keil:
   ```
   python tools/keil_setup.py
   ```
   Script tự: thêm nhóm file App/..., Include Paths, **IROM1 Size = 0xF800**, bật MicroLIB,
   chèn `App_Init()` / `App_Loop()` vào `main.c`. Chạy lại được nhiều lần (mỗi khi thêm file .c mới
   hoặc sau khi CubeMX sinh lại code). Bản gốc được giữ ở `*.bak`.

Mở `MDK-ARM/DryMachine.uvprojx` → **Build (F7)**.

---

## Cách làm tay (nếu không dùng file .ioc / script)

### A. STM32CubeMX

1. File → New Project → ô *Commercial Part Number* gõ **STM32F103C8T6** → chọn → Start Project.
2. **System Core → SYS**: Debug = **Serial Wire**. Timebase Source = SysTick.
3. **System Core → RCC**:
   * High Speed Clock (HSE) = **Crystal/Ceramic Resonator**
   * Low Speed Clock (LSE) = **Crystal/Ceramic Resonator** ← thạch anh 32.768 kHz, PC14/PC15 chuyển xanh
4. **Timers → RTC**:
   * Tick **Activate Clock Source**
   * **KHÔNG** tick *Activate Calendar* (firmware tự đọc bộ đếm RTC; bật Calendar thì HAL sẽ ghi đè giờ)
   * RTC OUT = No RTC Output
5. **Clock Configuration** (tab thứ 2):
   * PLL Source Mux = **HSE**, PLLMul = **x9**, System Clock Mux = **PLLCLK** → HCLK = 72 MHz
   * APB1 Prescaler = **/2** (36 MHz), ADC Prescaler = **/6** (12 MHz)
   * RTC Clock Mux = **LSE**
6. **Connectivity**:
   * **SPI1**: Mode = Full-Duplex Master, Prescaler = 4 (18 Mbit/s), CPOL Low, CPHA 1 Edge
   * **SPI2**: Mode = Full-Duplex Master, Prescaler = 16, **CPOL Low, CPHA 2 Edge**
   * **I2C2**: I2C, Standard Mode 100 kHz
   * **USART1**: Asynchronous, 115200 8N1
7. **Analog → ADC1**: tick **IN0**; Rank 1 Sampling Time = **239.5 Cycles**
8. **GPIO**: bấm chuột trái lên từng chân trong sơ đồ chip → chọn chế độ; chuột phải → *Enter User Label*.
   Rồi vào System Core → GPIO chỉnh mức khởi động / pull-up:

   | Chân | Chế độ | User Label | GPIO output level / Pull |
   |------|--------|------------|--------------------------|
   | PA0 | ADC1_IN0 | PRESS_ADC | – |
   | PA4 | GPIO_Output | TFT_CS | **High** |
   | PB0 | GPIO_Output | TFT_DC | Low |
   | PB1 | GPIO_Output | TFT_RST | **High** |
   | PB5 | GPIO_Output | RLY_COMP | **High** (relay nhả) |
   | PB6 | GPIO_Output | RLY_FAN_COND | **High** |
   | PB7 | GPIO_Output | RLY_FAN_EVAP | **High** |
   | PB12 | GPIO_Output | MAX_CS | **High** |
   | PC13 | GPIO_Output | LED_RUN | **High** (LED tắt) |
   | PA8 | GPIO_Input | BTN_ENTER | **Pull-up** |
   | PA15 | GPIO_Input | BTN_EXIT | **Pull-up** |
   | PB8 | GPIO_Input | BTN_UP | **Pull-up** |
   | PB9 | GPIO_Input | BTN_DOWN | **Pull-up** |

   Tên User Label phải gõ **chính xác** – code dùng các macro `RLY_COMP_Pin`, `RLY_COMP_GPIO_Port`…
9. **Project Manager**:
   * Project Name = `DryMachine`, Project Location = thư mục **cha** của repo
     (để CubeMX tạo đúng vào thư mục `2. DryMachine`)
   * **Toolchain / IDE = MDK-ARM**, Min Version V5.32
   * Linker Settings: Minimum Stack Size = **0x800**
   * Code Generator: tick *Generate peripheral initialization as a pair of '.c/.h' files per peripheral*
   * Tick *Keep User Code when re-generating*
10. **GENERATE CODE**.

### B. Keil µVision

Mở `MDK-ARM/DryMachine.uvprojx`.

1. **Thêm file** – trong cửa sổ Project, chuột phải *Target DryMachine* → **Manage Project Items…**
   * Cột *Groups*: bấm nút **New (Insert)** tạo lần lượt: `App/app`, `App/board`, `App/drivers`,
     `App/services`, `App/control`, `App/ui`
   * Chọn từng group → **Add Files…** → trỏ tới thư mục tương ứng, chọn tất cả file **.c**:

     | Group | File |
     |-------|------|
     | App/app | `App/app.c` |
     | App/board | `App/board/board.c` |
     | App/drivers | mọi `.c` trong `App/drivers/button`, `flash_store`, `ili9341` (2 file), `max31865`, `press_analog`, `relay`, `sht4x` |
     | App/services | `datetime.c`, `fault_log.c`, `log.c`, `presets.c`, `sched.c`, `sensors.c`, `settings.c`, `util_fmt.c` |
     | App/control | `dryer_ctrl.c` |
     | App/ui | `ui_core.c`, `ui_edit.c`, `ui_menu_preset.c`, `ui_menu_tech.c`, `ui_page_faults.c`, `ui_page_main.c`, `ui_page_run.c`, `ui_page_timer.c`, `ui_widgets.c` |

2. **Options for Target** (biểu tượng đũa thần, hoặc Alt+F7):
   * Tab **Target**:
     * ARM Compiler = *Use default compiler version 6*
     * Tick **Use MicroLIB**
     * Dòng **IROM1**: Start `0x8000000`, **Size `0xF800`** (mặc định là 0x10000)
   * Tab **C/C++ (AC6)**:
     * Language C = **gnu11**
     * **Include Paths** → bấm nút `...` → thêm từng thư mục, hoặc dán nối vào cuối ô (cách nhau bằng `;`):
       ```
       ../App;../App/board;../App/control;../App/services;../App/ui;../App/drivers/button;../App/drivers/relay;../App/drivers/max31865;../App/drivers/sht4x;../App/drivers/press_analog;../App/drivers/ili9341;../App/drivers/flash_store
       ```
   * Tab **Debug**: chọn *ST-Link Debugger* → Settings → Port = **SW**
   * Tab **Utilities**: tick *Use Debug Driver*
3. **Encoding**: Edit → Configuration → Editor → Encoding = **Encode in UTF-8 without signature**.
4. Sửa **`Core/Src/main.c`** – chỉ trong vùng USER CODE:

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
5. **Build (F7)** → **Download (F8)**.

> Tại sao IROM1 = 0xF800: Flash 64 KB, 2 page 1 KB cuối (0x0800F800 lịch sử lỗi, 0x0800FC00 thông số)
> được firmware ghi lúc chạy. Giảm IROM1 để trình liên kết không đặt code vào đó.

> Sinh lại code bằng CubeMX không làm mất Groups / Include Paths đã thêm trong Keil và code trong vùng
> USER CODE. Nếu mất (hiếm), chạy lại `python tools/keil_setup.py`.

## Kiểm tra nhanh trên PC (không cần board)

```sh
sh tools/host_check/check.sh
```
