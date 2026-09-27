# Sơ đồ chân STM32F103C8T6

| Chân | CubeMX mode | User Label | Nối tới |
|------|-------------|------------|---------|
| PA0  | ADC1_IN0 | PRESS_ADC | Cảm biến áp suất qua phân áp (xem dưới) |
| PA1  | GPIO_Output | RS485_DE | MAX485 DE + /RE (nối chung) |
| PA2  | USART2_TX | – | MAX485 DI |
| PA3  | USART2_RX | – | MAX485 RO |
| PA4  | GPIO_Output | TFT_CS | TFT CS |
| PA5  | SPI1_SCK | – | TFT SCK |
| PA6  | SPI1_MISO | – | TFT SDO (MISO) – không bắt buộc |
| PA7  | SPI1_MOSI | – | TFT SDI (MOSI) |
| PA8  | GPIO_Input (Pull-up) | BTN_ENTER | Nút ENTER → GND |
| PA9  | USART1_TX | – | Debug log 115200 (USB-UART) |
| PA10 | USART1_RX | – | Debug |
| PA13 | SYS_SWDIO | – | ST-Link |
| PA14 | SYS_SWCLK | – | ST-Link |
| PA15 | GPIO_Input (Pull-up) | BTN_EXIT | Nút EXIT → GND (cần SYS = Serial Wire) |
| PB0  | GPIO_Output | TFT_DC | TFT DC/RS |
| PB1  | GPIO_Output | TFT_RST | TFT RESET |
| PB5  | GPIO_Output | RLY_COMP | Relay máy nén |
| PB6  | GPIO_Output | RLY_FAN_COND | Relay quạt dàn nóng |
| PB7  | GPIO_Output | RLY_FAN_EVAP | Relay quạt dàn lạnh |
| PB8  | GPIO_Input (Pull-up) | BTN_UP | Nút UP → GND |
| PB9  | GPIO_Input (Pull-up) | BTN_DOWN | Nút DOWN → GND |
| PB12 | GPIO_Output | MAX_CS | MAX31865 CS |
| PB13 | SPI2_SCK | – | MAX31865 CLK |
| PB14 | SPI2_MISO | – | MAX31865 SDO |
| PB15 | SPI2_MOSI | – | MAX31865 SDI |
| PC13 | GPIO_Output | LED_RUN | LED trên board (nhấp nháy = chương trình đang chạy) |

Mức khởi động của các chân Output: **CS = High, RELAY = mức OFF, DE = Low**.

## Lưu ý phần cứng

* **Áp suất 0.5–4.5 V**: ADC STM32 chỉ chịu 3.3 V. Dùng phân áp R1 = 10 kΩ (nối tín hiệu) –
  R2 = 20 kΩ (xuống GND) → hệ số 1.5 (`PRESS_DIVIDER_RATIO`). Thêm tụ 100 nF tại PA0.
* **Relay**: Mặc định kích mức cao (`RELAY_ACTIVE_LEVEL` trong `app_config.h`). Module relay
  opto kích mức thấp thì đổi thành `GPIO_PIN_RESET`. Máy nén nên đóng qua contactor.
* **MAX31865**: điện trở tham chiếu module thường là 430 Ω (PT100). Chọn 2/3/4 dây bằng
  `MAX31865_WIRES` và hàn jumper trên module tương ứng.
* **Nút bấm**: nối chân → nút → GND, dùng pull-up nội. PA15 chỉ dùng được khi tắt JTAG
  (SYS → Debug = Serial Wire).
* **RS485**: điện trở 120 Ω đầu cuối nếu dây dài; GND chung với cảm biến.
