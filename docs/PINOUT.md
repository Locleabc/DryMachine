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
| PA1  | GPIO_Output (High) | RLY_FAN_S2 | Relay IN4 – quạt dàn nóng cấp 2 |
| PA2  | GPIO_Output (High) | RLY_FAN_S3 | Relay IN5 – quạt dàn nóng cấp 3 |
| PA3  | GPIO_Output (High) | RLY_FAN_S4 | Relay IN6 – quạt dàn nóng cấp 4 |
| PB4  | GPIO_Output (High) | RLY_FAN_S5 | Relay IN7 – quạt dàn nóng cấp 5 (cần SYS = Serial Wire) |
| PA13 | SYS_SWDIO | – | ST-Link |
| PA14 | SYS_SWCLK | – | ST-Link |
| PA15 | GPIO_Input (Pull-up) | BTN_EXIT | Nút EXIT → GND (cần SYS = Serial Wire) |
| PB0  | GPIO_Output | TFT_DC | TFT DC/RS |
| PB1  | GPIO_Output | TFT_RST | TFT RESET |
| PB10 | I2C2_SCL | – | SHT45 SCL (pull-up 4.7 kΩ lên 3.3 V) |
| PB11 | I2C2_SDA | – | SHT45 SDA (pull-up 4.7 kΩ lên 3.3 V) |
| PB5  | GPIO_Output (High) | RLY_COMP | Relay IN1 – máy nén (qua contactor) |
| PB6  | GPIO_Output (High) | RLY_FAN_S1 | Relay IN3 – quạt dàn nóng cấp 1 |
| PB7  | GPIO_Output (High) | RLY_FAN_EVAP | Relay IN2 – quạt dàn lạnh |
| PB8  | GPIO_Input (Pull-up) | BTN_UP | Nút UP → GND |
| PB9  | GPIO_Input (Pull-up) | BTN_DOWN | Nút DOWN → GND |
| PB12 | GPIO_Output | MAX_CS | MAX31865 CS |
| PB13 | SPI2_SCK | – | MAX31865 CLK |
| PB14 | SPI2_MISO | – | MAX31865 SDO |
| PB15 | SPI2_MOSI | – | MAX31865 SDI |
| PC14 / PC15 | RCC_OSC32_IN / OUT | – | Thạch anh 32.768 kHz có sẵn trên Blue Pill (RTC) |
| VBAT (VB) | – | – | Pin CR2032 (+) để giữ giờ khi mất điện; (−) nối GND |
| PC13 | GPIO_Output | LED_RUN | LED trên board (nhấp nháy = chương trình đang chạy) |

Mức khởi động của các chân Output: **CS = High, 7 chân RELAY = High (relay kích mức thấp → nhả)**.

Chân trống dự phòng: PB3 (+ IN8 của module relay 8 kênh).

## Quạt dàn nóng 5 cấp

Motor quạt nhiều đầu dây tốc độ: dây chung (COM motor) lên N/L theo sơ đồ motor, mỗi đầu dây tốc độ đi qua
tiếp điểm NO của 1 relay (IN3…IN7). Firmware chỉ đóng **đúng 1** relay tốc độ, khi đổi cấp thì tắt hết
1 giây rồi mới đóng cấp mới (`control/fan_speed.c`). Nên thêm khoá liên động cứng (dùng tiếp điểm NC nối tiếp)
nếu motor không chịu được 2 đầu dây cùng có điện khi relay dính.

## Lưu ý phần cứng

* **Áp suất 0.5–4.5 V**: ADC STM32 chỉ chịu 3.3 V. Dùng phân áp R1 = 10 kΩ (nối tín hiệu) –
  R2 = 20 kΩ (xuống GND) → hệ số 1.5 (`PRESS_DIVIDER_RATIO`). Thêm tụ 100 nF tại PA0.
* **Relay**: module 5V opto kích mức THẤP. Tháo jumper VCC–JD-VCC: VCC (phía opto) nối 3.3V
  Blue Pill, JD-VCC nối 5V buck. K1 chỉ đóng cuộn contactor, contactor mới cấp cho máy nén.
  Đấu RC snubber / varistor song song cuộn contactor và quạt.
* **MAX31865**: điện trở tham chiếu module thường là 430 Ω (PT100; module PT1000 là 4300 Ω).
  Firmware mặc định **PT100 3 dây** (`board_pt100.wires = 3` trong `App/board/board.c`) – module phải hàn jumper 3 dây
  (cắt nối "2/3 wire", hàn "3" và "24|3"). PT100 2 hoặc 4 dây: để jumper mặc định và đổi `wires = 2` / `4`.
  Nguồn VIN 3.3–5 V, SDI → PB15, SDO → PB14, CLK → PB13, CS → PB12.
  Lỗi hiện ở trang chính:
  | Thông báo | Nguyên nhân thường gặp |
  |-----------|------------------------|
  | *MAX31865 không phản hồi (SPI2)* | Chưa cấp nguồn, sai/đảo SDI–SDO, CS không nối PB12 |
  | *PT100 hở mạch / đứt dây* | Chưa nối PT100, đứt dây, bắt vít lỏng |
  | *PT100 chập mạch* | Hai dây PT100 chạm nhau |
  | *PT100 hở hoặc sai jumper 2/3/4 dây* | Jumper module không khớp số dây (`wires`) hoặc thiếu dây thứ 3 |
  | *PT100: giá trị ngoài dải (sai Rref?)* | Module PT1000 (Rref 4300) nhưng firmware đặt 430, hoặc ngược lại |
* **Nút bấm**: nối chân → nút → GND, dùng pull-up nội. PA15 chỉ dùng được khi tắt JTAG
  (SYS → Debug = Serial Wire).
* **SHT45** (I2C, địa chỉ 0x44 với mã SHT45-AD1B, 0x45 với BD1B): nguồn 3.3 V, tụ 100 nF sát
  cảm biến. I2C không hợp chạy dây dài: giữ dây ≤ 1 m, dùng cáp xoắn (SDA+GND, SCL+VCC);
  xa hơn thì dùng IC mở rộng bus (P82B715 / PCA9615). Cảm biến chịu -40…125 °C.
  Không đặt cảm biến ngay luồng gió nóng ra dàn nóng; nên có vỏ lọc bụi (PTFE) để tránh bụi thực phẩm.
* **Cảm biến ẩm**: firmware tự nhận **SHT4x (SHT40/41/45)** hoặc **SHT3x (SHT30/31/35, module SHT31-D)**, địa chỉ 0x44 hoặc 0x45.
  Module SHT31-D (GY-SHT31-D, Adafruit) có sẵn ổn áp + điện trở kéo lên → VIN 3.3–5 V, SCL → PB10, SDA → PB11, ADDR để trống (0x44).
* **Cảm biến ẩm không đọc được** – dòng cảnh báo trang chính cho biết lý do:
  | Thông báo | Nguyên nhân thường gặp |
  |-----------|------------------------|
  | *SHT không trả lời – kiểm tra dây/nguồn* | Chưa cấp nguồn / sai chân (SCL = **PB10**, SDA = **PB11**, hay bị đảo), GND chưa chung, đứt dây. Firmware tự thử cả 0x44 và 0x45 |
  | *SHT: bus I2C bị giữ thấp (SDA/SCL)* | Thiếu điện trở kéo lên 4.7 kΩ lên 3.3 V, chập SDA/SCL xuống GND, cảm biến hỏng |
  | *SHT: sai CRC – nhiễu / dây dài* | Dây quá dài / gần dây động lực quạt, máy nén |
  | *SHT: lỗi I2C (timeout)* | Pull-up quá yếu (> 10 kΩ), dây dài, tụ lớn trên đường tín hiệu |
  Đo bằng đồng hồ khi cấp điện, chưa giao tiếp: SDA và SCL phải ≈ 3.3 V. Nguồn SHT45 **tối đa 3.6 V** –
  module trần không có ổn áp mà cấp 5 V có thể đã hỏng cảm biến. Log UART lúc khởi động in `SHT3x @0x44 st0 …` (3x/4x = loại cảm biến nhận được)
  (st0 = OK, 1 lỗi I2C, 2 CRC, 3 không trả lời, 4 bus bận).
