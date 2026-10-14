# Sơ đồ chân STM32F103C8T6

Sơ đồ đấu dây trực quan (mở bằng trình duyệt): [wiring.html](wiring.html)

## Nguồn

220VAC → CB 10A + cầu chì → nguồn tổ ong 12V → buck 5V (≥ 2A) → chân 5V Blue Pill, JD-VCC relay,
VCC TFT. 3.3V lấy từ LDO trên Blue Pill cho SHT45, phân áp 2 NTC, VCC opto relay.

## Gán chân

| Chân | CubeMX mode | User Label | Nối tới |
|------|-------------|------------|---------|
| PA0  | ADC1_IN0 | NTC_EVAP1 | **NTC nhiệt độ buồng sấy** (điều khiển, thay PT100) – phân áp 10k lên 3.3V (xem dưới) |
| PA4  | GPIO_Output | TFT_CS | TFT CS |
| PA5  | SPI1_SCK | – | TFT SCK |
| PA6  | ADC1_IN6 | NTC_EVAP2 | **NTC dàn lạnh** (chống đóng băng) – phân áp 10k lên 3.3V. SPI1 chỉ phát → TFT SDO để trống |
| PA7  | SPI1_MOSI | – | TFT SDI (MOSI) |
| PA11 | GPIO_Output (cấu hình trong `board.c`) | – | TFT **LED** (đèn nền, cao = sáng) – tự tắt màn hình |
| PA8  | GPIO_Input (Pull-up) | BTN_ENTER | Nút ENTER → GND |
| PA9  | USART1_TX | – | Debug log 115200 (USB-UART) |
| PA10 | USART1_RX | – | Debug |
| PA1  | GPIO_Output (High) | RLY_FAN_S3 | Relay IN5 – quạt dàn nóng S3 (cấp ≥ 3) |
| PA2  | GPIO_Output (High) | RLY_FAN_S4 | Relay IN6 – quạt dàn nóng S4 (cấp ≥ 4) |
| PA3  | GPIO_Output (High) | RLY_FAN_S5 | Relay IN7 – quạt dàn nóng S5 (cấp 5) |
| PB4  | GPIO_Output (**Low**) | RLY_COMP | Máy nén (qua contactor) – **ĐẢO: mức CAO = đóng**. Thêm điện trở 10 kΩ PB4 → GND (cần SYS = Serial Wire) |
| PA13 | SYS_SWDIO | – | ST-Link |
| PA14 | SYS_SWCLK | – | ST-Link |
| PA15 | GPIO_Input (Pull-up) | BTN_EXIT | Nút EXIT → GND (cần SYS = Serial Wire) |
| PB0  | GPIO_Output | TFT_DC | TFT DC/RS |
| PB1  | GPIO_Output | TFT_RST | TFT RESET |
| PB10 | I2C2_SCL | – | SHT45 SCL (pull-up 4.7 kΩ lên 3.3 V) |
| PB11 | I2C2_SDA | – | SHT45 SDA (pull-up 4.7 kΩ lên 3.3 V) |
| PB5  | GPIO_Output (High) | RLY_FAN_EVAP | Relay IN2 – quạt dàn lạnh |
| PB6  | GPIO_Output (High) | RLY_FAN_S1 | Relay IN3 – quạt dàn nóng S1 (cấp ≥ 1) |
| PB7  | GPIO_Output (High) | RLY_FAN_S2 | Relay IN4 – quạt dàn nóng S2 (cấp ≥ 2) |
| PB8  | GPIO_Input (Pull-up) | BTN_UP | Nút UP → GND |
| PB9  | GPIO_Input (Pull-up) | BTN_DOWN | Nút DOWN → GND |
| PC14 / PC15, VBAT | – | – | Không dùng (đã bỏ đồng hồ thời gian thực – không cần pin CR2032) |
| PC13 | GPIO_Output | LED_RUN | LED trên board (nhấp nháy = chương trình đang chạy) |

Mức khởi động của các chân Output: **CS = High, 6 chân relay quạt = High (kích mức thấp → nhả), PB4 máy nén = Low (kích mức cao → nhả)**.

⚠ PB4 sau reset là chân JTAG NJTRST có **pull-up nội** cho tới khi firmware cấu hình (vài ms). Với tầng kích mức cao, thêm
**10 kΩ từ PB4 xuống GND** để máy nén chắc chắn không nhấp lúc cấp điện / reset / nạp code.

Chân trống dự phòng: PB3 (+ IN8 của module relay 8 kênh), PB12–PB15 (đã bỏ MAX31865 / PT100).

## Quạt dàn nóng 5 cấp

Kiểu **cộng dồn** (`control/fan_speed.c`): cấp N đóng relay IN3…(IN3+N−1) – cấp 1 = 1 relay, cấp 5 = cả 5 relay.
Tăng cấp: đóng thêm từng relay, cách nhau 1 s (giảm dòng khởi động). Giảm cấp / tắt: nhả ngay các relay thừa.

| Cấp | IN3 (S1) | IN4 (S2) | IN5 (S3) | IN6 (S4) | IN7 (S5) |
|-----|----------|----------|----------|----------|----------|
| 1 | ON | | | | |
| 2 | ON | ON | | | |
| 3 | ON | ON | ON | | |
| 4 | ON | ON | ON | ON | |
| 5 | ON | ON | ON | ON | ON |

⚠ Mỗi relay phải cấp cho **một tải riêng** (5 quạt / 5 nhánh song song). **Không** dùng cho motor nhiều đầu dây tốc độ:
nhiều đầu dây tốc độ cùng có điện sẽ chập cuộn dây motor.

## Lưu ý phần cứng

* **2 NTC: buồng sấy (PA0) + dàn lạnh (PA6)** (R25 / B từng cái: xem dưới), mỗi đầu dò: 3.3V – điện trở **10 kΩ 1%** (`r_fixed` trong `board.c`) – chân ADC (PA0 / PA6) – NTC – GND,
  thêm tụ **100 nF** từ chân ADC xuống GND. 25 °C ≈ 2.2 V, 0 °C ≈ 2.87 V, −10 °C ≈ 3.04 V, hở dây ≈ 3.3 V (firmware báo *Mất NTC*).
  Đầu dò dàn lạnh kẹp sát ống đồng, bọc cách nhiệt. R25 và hệ số B **riêng từng đầu dò** khai báo trong `App/board/board.c` (hiện: buồng (PA0) = 14 kΩ / B 2800,
  dàn lạnh (PA6) = 20 kΩ / B 3000 – đo thực tế). Thay đầu dò: đo điện trở ở nước đá đang tan (0 °C) và ở nhiệt độ phòng T (°C), rồi
  B = ln(R0 / RT) / (1/273.15 − 1/(T+273.15)), R25 = RT · e^(B·(1/298.15 − 1/(T+273.15))). Lệch nhỏ còn lại: *Bù nhiệt buồng* / *Bù dàn lạnh* (menu kỹ thuật).
* **Relay**: module 5V opto kích mức THẤP. Tháo jumper VCC–JD-VCC: VCC (phía opto) nối 3.3V
  Blue Pill, JD-VCC nối 5V buck. K1 chỉ đóng cuộn contactor, contactor mới cấp cho máy nén.
  Đấu RC snubber / varistor song song cuộn contactor và quạt.
* **Đèn nền TFT (PA11)**: module ILI9341 SPI 2.4"/2.8" (bo đỏ) thường có transistor đệm ở chân LED → nối thẳng PA11.
  Nếu chân LED nối thẳng vào LED nền qua điện trở (không có transistor, dòng > 20 mA) thì đệm bằng NPN (S8050/2N2222,
  base qua 1 kΩ từ PA11) hoặc MOSFET logic-level. Không nối PA11 thì vẫn để LED lên 3.3 V như cũ – tự tắt chỉ làm đen màn hình.
* **Nhiệt độ buồng sấy = NTC ở PA0** (đã bỏ MAX31865 + PT100). Mạch giống NTC dàn lạnh (phân áp 10 kΩ lên 3.3V + 100 nF).
  Lỗi *Mất cảm biến nhiệt* (dừng máy) khi NTC hở/chập liên tục > 3 s; dòng lỗi ghi *NTC buồng (PA0) hở dây – 3.30V* /
  *chập – 0.00V* (điện áp lúc hỏng). Lỗi được **giữ** tới khi xoá tay (ENTER ở trang 4); đọc lại được thì dòng lỗi luân phiên
  *NTC buồng đã OK lại · xoá lỗi ở trang 4*. Log UART: `NTC buong hong: adc … (lan N)`.
  ⚠ Đầu dò buồng nằm trong khí nóng tới ~75 °C (bảo vệ quá nhiệt): dùng NTC vỏ inox/epoxy chịu nhiệt, không dùng loại bọc nhựa PVC mỏng.
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
