# Tạo project trong STM32CubeIDE

1. File → New → STM32 Project → chọn **STM32F103C8Tx**.
   Project name: `DryMachine`, **bỏ tick "Use default location"** và trỏ tới thư mục repo này.
2. Cấu hình trong file `.ioc`:
   * **SYS**: Debug = *Serial Wire*, Timebase = SysTick
   * **RCC**: HSE = Crystal/Ceramic Resonator
   * **Clock**: HSE 8 MHz → PLL x9 → SYSCLK 72 MHz, APB1 36 MHz, APB2 72 MHz, ADC prescaler /6 (12 MHz)
   * **SPI1** (TFT): Transmit Only Master (hoặc Full-Duplex Master), 8 bit, CPOL Low, CPHA 1 Edge,
     NSS Software, Prescaler 4 (18 Mbit/s)
   * **SPI2** (MAX31865): Full-Duplex Master, 8 bit, MSB first, **CPOL Low, CPHA 2 Edge (mode 1)**,
     NSS Software, Prescaler 16 (2.25 Mbit/s)
   * **USART1**: Asynchronous 115200 8N1 (debug)
   * **USART2**: Asynchronous 9600 8N1, **NVIC: bật USART2 global interrupt**
   * **ADC1**: IN0, Continuous = Disabled, Sampling time 239.5 cycles
   * **GPIO**: theo [PINOUT.md](PINOUT.md) – đặt đúng *User Label* (code dùng tên macro
     `RLY_COMP_Pin`, `RLY_COMP_GPIO_Port`, …)
   * Project Manager → Code Generator: tick *Generate peripheral initialization as a pair of .c/.h* (tuỳ chọn)
3. Generate code.
4. Thêm thư mục App vào build:
   * Chuột phải project → Properties → C/C++ General → Paths and Symbols
     * **Includes** (GNU C): thêm `App/Inc`
     * **Source Location**: Add Folder → `App`
5. Sửa `Core/Src/main.c` – chỉ trong vùng USER CODE:

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

6. Build (Ctrl+B) → Flash qua ST-Link.

> Nếu Flash không đủ khi build Debug (-O0), vào Properties → C/C++ Build → Settings →
> MCU GCC Compiler → Optimization chọn **-Os**.
