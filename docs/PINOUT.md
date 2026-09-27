# Sơ đồ chân STM32F103C8T6

Sơ đồ đấu dây trực quan (mở bằng trình duyệt): [wiring.html](wiring.html)

## Nguồn

220VAC → CB 10A + cầu chì → nguồn tổ ong 12V → buck 5V (≥ 2A) → chân 5V Blue Pill, JD-VCC relay,
VCC TFT, nguồn cảm biến áp. 3.3V lấy từ LDO trên Blue Pill cho MAX31865, SHT45, VCC opto relay.

## Gán chân

| Chân | CubeMX mode | User Label | Nối tới |
|------|-------------|------------|---------|
| PA0  | ADC1_IN0 | PRESS_ADC | Cảm biến áp suất qua phân áp (xem dưới) |
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
| PB10 | I2C2_SCL | – | SHT45 SCL (pull-up 4.7 kΩ lên 3.3 V) |
| PB11 | I2C2_SDA | – | SHT45 SDA (pull-up 4.7 kΩ lên 3.3 V) |
| PB5  | GPIO_Output (High) | RLY_COMP | Relay IN1 – máy nén (qua contactor) |
| PB6  | GPIO_Output (High) | RLY_FAN_COND | Relay IN2 – quạt dàn nóng |
| PB7  | GPIO_Output (High) | RLY_FAN_EVAP | Relay IN3 – quạt dàn lạnh |
| PB8  | GPIO_Input (Pull-up) | BTN_UP | Nút UP → GND |
| PB9  | GPIO_Input (Pull-up) | BTN_DOWN | Nút DOWN → GND |
| PB12 | GPIO_Output | MAX_CS | MAX31865 CS |
| PB13 | SPI2_SCK | – | MAX31865 CLK |
| PB14 | SPI2_MISO | – | MAX31865 SDO |
| PB15 | SPI2_MOSI | – | MAX31865 SDI |
| PC14 / PC15 | RCC_OSC32_IN / OUT | – | Thạch anh 32.768 kHz có sẵn trên Blue Pill (RTC) |
| VBAT (VB) | – | – | Pin CR2032 (+) để giữ giờ khi mất điện; (−) nối GND |
| PC13 | GPIO_Output | LED_RUN | LED trên board (nhấp nháy = chương trình đang chạy) |

Mức khởi động của các chân Output: **CS = High, RELAY (PB5–PB7) = High (relay kích mức thấp → nhả)**.

Chân trống dự phòng: PA1, PA2, PA3 (USART2 nếu sau này cần RS485), PB3, PB4.

## Lưu ý phần cứng

* **Áp suất 0.5–4.5 V**: ADC STM32 chỉ chịu 3.3 V. Dùng phân áp R1 = 10 kΩ (nối tín hiệu) –
  R2 = 20 kΩ (xuống GND) → hệ số 1.5 (`PRESS_DIVIDER_RATIO`). Thêm tụ 100 nF tại PA0.
* **Relay**: module 5V opto kích mức THẤP. Tháo jumper VCC–JD-VCC: VCC (phía opto) nối 3.3V
  Blue Pill, JD-VCC nối 5V buck. K1 chỉ đóng cuộn contactor, contactor mới cấp cho máy nén.
  Đấu RC snubber / varistor song song cuộn contactor và quạt.
* **MAX31865**: điện trở tham chiếu module thường là 430 Ω (PT100). Chọn 2/3/4 dây bằng
  `MAX31865_WIRES` và hàn jumper trên module tương ứng.
* **Nút bấm**: nối chân → nút → GND, dùng pull-up nội. PA15 chỉ dùng được khi tắt JTAG
  (SYS → Debug = Serial Wire).
* **SHT45** (I2C, địa chỉ 0x44 với mã SHT45-AD1B, 0x45 với BD1B): nguồn 3.3 V, tụ 100 nF sát
  cảm biến. I2C không hợp chạy dây dài: giữ dây ≤ 1 m, dùng cáp xoắn (SDA+GND, SCL+VCC);
  xa hơn thì dùng IC mở rộng bus (P82B715 / PCA9615). Cảm biến chịu -40…125 °C.
  Không đặt cảm biến ngay luồng gió nóng ra dàn nóng; nên có vỏ lọc bụi (PTFE) để tránh bụi thực phẩm.
