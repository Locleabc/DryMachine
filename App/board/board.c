/**
 * @file    board.c
 * @brief   Cấu hình phần cứng thực tế. Tên chân (xxx_Pin / xxx_GPIO_Port) lấy từ
 *          User Label trong CubeMX (docs/PINOUT.md).
 */
#include "board.h"
#include "main.h"

/* Handle ngoại vi do CubeMX định nghĩa (spi.c, i2c.c, adc.c, usart.c hoặc main.c).
 * Khai báo extern tại đây để không phụ thuộc tuỳ chọn "pair .c/.h" của CubeMX. */
extern SPI_HandleTypeDef  hspi1;
extern I2C_HandleTypeDef  hi2c2;
extern ADC_HandleTypeDef  hadc1;
extern UART_HandleTypeDef huart1;

/* ---------------- Relay ----------------
 * Module relay 8 kênh 5V opto, kích mức THẤP: VCC (opto) = 3.3V, JD-VCC = 5V buck, tháo jumper.
 * CubeMX: mức khởi động các chân relay = High để relay nhả khi cấp điện.
 * Máy nén PB4 (IN1) – ĐẢO: mức CAO = đóng (relay/SSR kích mức cao), khởi động = Low.
 * Quạt dàn lạnh PB5 · quạt dàn nóng S1..S5 = PB6, PB7, PA1, PA2, PA3 (cộng dồn).
 * Nếu đổi sang module kích mức cao: sửa thành GPIO_PIN_SET và mức khởi động = Low. */
const relay_hw_t board_relays[RLY_ID_COUNT] = {
    [RLY_ID_COMP]     = { RLY_COMP_GPIO_Port,     RLY_COMP_Pin,     GPIO_PIN_SET   },   /* đảo: cao = ON */
    [RLY_ID_FAN_EVAP] = { RLY_FAN_EVAP_GPIO_Port, RLY_FAN_EVAP_Pin, GPIO_PIN_RESET },
    [RLY_ID_FAN_S1]   = { RLY_FAN_S1_GPIO_Port,   RLY_FAN_S1_Pin,   GPIO_PIN_RESET },
    [RLY_ID_FAN_S2]   = { RLY_FAN_S2_GPIO_Port,   RLY_FAN_S2_Pin,   GPIO_PIN_RESET },
    [RLY_ID_FAN_S3]   = { RLY_FAN_S3_GPIO_Port,   RLY_FAN_S3_Pin,   GPIO_PIN_RESET },
    [RLY_ID_FAN_S4]   = { RLY_FAN_S4_GPIO_Port,   RLY_FAN_S4_Pin,   GPIO_PIN_RESET },
    [RLY_ID_FAN_S5]   = { RLY_FAN_S5_GPIO_Port,   RLY_FAN_S5_Pin,   GPIO_PIN_RESET },
};

/* ---------------- Nút bấm (pull-up nội, nhấn = GND) ---------------- */
const button_hw_t board_buttons[BTN_ID_COUNT] = {
    [BTN_ID_UP]    = { BTN_UP_GPIO_Port,    BTN_UP_Pin,    GPIO_PIN_RESET, true  },
    [BTN_ID_DOWN]  = { BTN_DOWN_GPIO_Port,  BTN_DOWN_Pin,  GPIO_PIN_RESET, true  },
    [BTN_ID_ENTER] = { BTN_ENTER_GPIO_Port, BTN_ENTER_Pin, GPIO_PIN_RESET, false },
    [BTN_ID_EXIT]  = { BTN_EXIT_GPIO_Port,  BTN_EXIT_Pin,  GPIO_PIN_RESET, false },
};

/* ---------------- SHT45 (I2C2) ---------------- */
const sht4x_cfg_t board_sht45 = {
    .hi2c = &hi2c2, .addr = SHT4X_ADDR_A, .precision = SHT4X_PREC_HIGH,
};

/* ---------------- NTC (ADC1) – phân áp 10k 1% lên 3.3V ----------------
 * code < 20: chập · code > 4050: hở / chưa cắm (< ~ −40 °C) */
#define NTC_COMMON  .hadc = &hadc1, .r_fixed = 10000.0f, .adc_max = 4095.0f, \
                    .code_min = 20, .code_max = 4050, .oversample = 8
/* R25 / B theo đo thực tế đầu dò đang lắp (nước đá 0 °C và 28 °C):
 *   PA0 – NHIỆT ĐỘ BUỒNG (điều khiển, thay PT100): 33 kΩ @0 °C, 12.7 kΩ @28 °C → R25 ≈ 14 kΩ, B ≈ 2800
 *   PA6 – DÀN LẠNH (chống đóng băng):              50 kΩ @0 °C, 18.0 kΩ @28 °C → R25 ≈ 20 kΩ, B ≈ 3000
 * Thay đầu dò khác loại → đo lại 2 điểm và sửa ở đây. Sai lệch nhỏ: "Bù nhiệt buồng" / "Bù dàn lạnh". */
const ntc_cfg_t board_ntc_chamber = { NTC_COMMON, .channel = ADC_CHANNEL_0, .r25 = 14000.0f, .beta = 2800.0f };  /* PA0 */
const ntc_cfg_t board_ntc_evap    = { NTC_COMMON, .channel = ADC_CHANNEL_6, .r25 = 20000.0f, .beta = 3000.0f };  /* PA6 */

/* ---------------- TFT ILI9341 (SPI1) ---------------- */
const ili9341_cfg_t board_lcd = {
    .hspi = &hspi1,
    .cs_port  = TFT_CS_GPIO_Port,  .cs_pin  = TFT_CS_Pin,
    .dc_port  = TFT_DC_GPIO_Port,  .dc_pin  = TFT_DC_Pin,
    .rst_port = TFT_RST_GPIO_Port, .rst_pin = TFT_RST_Pin,
};

/* ---------------- Flash: page 62 = lịch sử lỗi, page 63 = thông số ----------------
 * Keil: Options for Target → Target → IROM1 Size = 0xF800 (chừa 2 page cuối) */
const flash_store_cfg_t board_faultlog_flash = { .addr = 0x0800F800UL, .size = 0x400 };
const flash_store_cfg_t board_settings_flash = { .addr = 0x0800FC00UL, .size = 0x400 };

/* ---------------- Tiện ích board ---------------- */
uint32_t Board_Millis(void)
{
    return HAL_GetTick();
}

/* Đèn nền TFT: chân LED của module → PA11 (đẩy-kéo, mức cao = sáng).
 * Cấu hình ngay tại đây (không cần sửa CubeMX). Chưa nối PA11 thì màn hình vẫn tắt bằng cách tô đen. */
#define LCD_BL_PORT  GPIOA
#define LCD_BL_PIN   GPIO_PIN_11

void Board_LcdBacklight(bool on)
{
    static bool init;
    HAL_GPIO_WritePin(LCD_BL_PORT, LCD_BL_PIN, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
    if (!init) {
        GPIO_InitTypeDef g = {0};
        __HAL_RCC_GPIOA_CLK_ENABLE();
        g.Pin   = LCD_BL_PIN;
        g.Mode  = GPIO_MODE_OUTPUT_PP;
        g.Pull  = GPIO_NOPULL;
        g.Speed = GPIO_SPEED_FREQ_LOW;
        HAL_GPIO_Init(LCD_BL_PORT, &g);
        init = true;
    }
}

void Board_LedToggle(void)
{
    HAL_GPIO_TogglePin(LED_RUN_GPIO_Port, LED_RUN_Pin);
}

void Board_LogWrite(const char *data, uint16_t len)
{
    HAL_UART_Transmit(&huart1, (const uint8_t *)data, len, 50);
}

static void i2c_delay(void)
{
    for (volatile int i = 0; i < 60; i++) { }     /* ~5 µs @72 MHz */
}

/* Bus I2C bị treo (SDA bị slave giữ thấp, hoặc lỗi BUSY của I2C STM32F1):
 * phát 9 xung SCL + STOP bằng GPIO, reset khối I2C rồi khởi tạo lại. */
void Board_I2cRecover(void)
{
    const uint16_t SCL = GPIO_PIN_10, SDA = GPIO_PIN_11;   /* I2C2 = PB10 / PB11 */
    GPIO_InitTypeDef g = {0};

    HAL_I2C_DeInit(&hi2c2);
    g.Pin = SCL | SDA;
    g.Mode = GPIO_MODE_OUTPUT_OD;
    g.Pull = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &g);

    HAL_GPIO_WritePin(GPIOB, SDA, GPIO_PIN_SET);
    for (int i = 0; i < 9; i++) {
        HAL_GPIO_WritePin(GPIOB, SCL, GPIO_PIN_RESET); i2c_delay();
        HAL_GPIO_WritePin(GPIOB, SCL, GPIO_PIN_SET);   i2c_delay();
    }
    HAL_GPIO_WritePin(GPIOB, SDA, GPIO_PIN_RESET); i2c_delay();   /* STOP */
    HAL_GPIO_WritePin(GPIOB, SDA, GPIO_PIN_SET);   i2c_delay();

    __HAL_RCC_I2C2_FORCE_RESET();
    __HAL_RCC_I2C2_RELEASE_RESET();
    HAL_I2C_Init(&hi2c2);                           /* MspInit cấu hình lại chân AF */
}
