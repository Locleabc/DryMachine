/**
 * @file    board.c
 * @brief   Cấu hình phần cứng thực tế. Tên chân (xxx_Pin / xxx_GPIO_Port) lấy từ
 *          User Label trong CubeMX (docs/PINOUT.md).
 */
#include "board.h"
#include "main.h"

/* Handle ngoại vi do CubeMX định nghĩa (spi.c, i2c.c, adc.c, usart.c hoặc main.c).
 * Khai báo extern tại đây để không phụ thuộc tuỳ chọn "pair .c/.h" của CubeMX. */
extern SPI_HandleTypeDef  hspi1, hspi2;
extern I2C_HandleTypeDef  hi2c2;
extern ADC_HandleTypeDef  hadc1;
extern UART_HandleTypeDef huart1;

/* ---------------- Relay ----------------
 * Module relay 8 kênh 5V opto, kích mức THẤP: VCC (opto) = 3.3V, JD-VCC = 5V buck, tháo jumper.
 * CubeMX: mức khởi động các chân relay = High để relay nhả khi cấp điện.
 * Quạt dàn nóng 5 cấp: S1 PB6, S2 PA1, S3 PA2, S4 PA3, S5 PB4 (motor nhiều đầu dây tốc độ).
 * Nếu đổi sang module kích mức cao: sửa thành GPIO_PIN_SET và mức khởi động = Low. */
const relay_hw_t board_relays[RLY_ID_COUNT] = {
    [RLY_ID_COMP]     = { RLY_COMP_GPIO_Port,     RLY_COMP_Pin,     GPIO_PIN_RESET },
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

/* ---------------- PT100 / MAX31865 (SPI2) ---------------- */
const max31865_cfg_t board_pt100 = {
    .hspi = &hspi2, .cs_port = MAX_CS_GPIO_Port, .cs_pin = MAX_CS_Pin,
    .rref = 430.0f, .r0 = 100.0f, .wires = 3, .filter_50hz = true,
};

/* ---------------- SHT45 (I2C2) ---------------- */
const sht4x_cfg_t board_sht45 = {
    .hi2c = &hi2c2, .addr = SHT4X_ADDR_A, .precision = SHT4X_PREC_HIGH,
};

/* ---------------- Áp suất 0.5–4.5 V qua phân áp 10k/20k (ADC1_IN0) ---------------- */
const press_analog_cfg_t board_press = {
    .hadc = &hadc1, .vref = 3.3f, .adc_max = 4095.0f, .divider = 1.5f,
    .v_min = 0.5f, .v_max = 4.5f,
    .p_min = 0.0f, .p_max = 34.5f,          /* 500 psi – chỉnh theo cảm biến thực tế */
    .v_fault_low = 0.25f, .v_fault_high = 4.8f,
    .oversample = 8,
};

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

/* ---------------- RTC ----------------
 * Dùng thẳng bộ đếm 32 bit RTC_CNT (1 tick = 1 s, prescaler do MX_RTC_Init đặt).
 * Không dùng HAL_RTC_GetDate của dòng F1 vì HAL F1 không giữ ngày qua reset.
 * BKP_DR1 = RTC_MAGIC đánh dấu đã chỉnh giờ (giữ nhờ pin VBAT). */
#define RTC_MAGIC   0xA5A5U

void Board_RtcInit(void)
{
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_RCC_BKP_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();
}

bool Board_RtcRead(uint32_t *seconds)
{
    uint16_t hi1 = (uint16_t)RTC->CNTH;
    uint16_t lo  = (uint16_t)RTC->CNTL;
    uint16_t hi2 = (uint16_t)RTC->CNTH;
    if (hi1 != hi2) lo = (uint16_t)RTC->CNTL;        /* CNTL vừa tràn giữa 2 lần đọc */
    *seconds = ((uint32_t)hi2 << 16) | lo;
    return (BKP->DR1 & 0xFFFFU) == RTC_MAGIC;
}

static void rtc_wait_ready(void)
{
    uint32_t t0 = HAL_GetTick();
    while (!(RTC->CRL & RTC_CRL_RTOFF) && (HAL_GetTick() - t0) < 10) { }
}

void Board_RtcWrite(uint32_t seconds)
{
    rtc_wait_ready();
    RTC->CRL |= RTC_CRL_CNF;                          /* vào chế độ cấu hình */
    RTC->CNTH = seconds >> 16;
    RTC->CNTL = seconds & 0xFFFFU;
    RTC->CRL &= ~RTC_CRL_CNF;
    rtc_wait_ready();
    BKP->DR1 = RTC_MAGIC;
}
