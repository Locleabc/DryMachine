/**
 * @file    drv_max31865.c
 */
#include "drv_max31865.h"
#include "app_config.h"
#include <math.h>

/* Thanh ghi */
#define REG_CONFIG      0x00
#define REG_RTD_MSB     0x01
#define REG_FAULT       0x07
#define WRITE_FLAG      0x80

/* Bit cấu hình */
#define CFG_VBIAS       0x80
#define CFG_AUTO        0x40
#define CFG_3WIRE       0x10
#define CFG_FAULTCLR    0x02
#define CFG_50HZ        0x01

/* Hệ số Callendar–Van Dusen (IEC 60751) */
#define CVD_A           3.9083e-3f
#define CVD_B          -5.775e-7f

#define SPI_TIMEOUT     10

static inline void cs_low(void)  { HAL_GPIO_WritePin(MAX_CS_GPIO_Port, MAX_CS_Pin, GPIO_PIN_RESET); }
static inline void cs_high(void) { HAL_GPIO_WritePin(MAX_CS_GPIO_Port, MAX_CS_Pin, GPIO_PIN_SET); }

static void write_reg(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = { (uint8_t)(reg | WRITE_FLAG), val };
    cs_low();
    HAL_SPI_Transmit(MAX31865_SPI, buf, 2, SPI_TIMEOUT);
    cs_high();
}

static void read_regs(uint8_t reg, uint8_t *dst, uint8_t len)
{
    uint8_t addr = reg & 0x7F;
    cs_low();
    HAL_SPI_Transmit(MAX31865_SPI, &addr, 1, SPI_TIMEOUT);
    HAL_SPI_Receive(MAX31865_SPI, dst, len, SPI_TIMEOUT);
    cs_high();
}

static uint8_t base_config(void)
{
    uint8_t cfg = CFG_VBIAS | CFG_AUTO;
#if (MAX31865_WIRES == 3)
    cfg |= CFG_3WIRE;
#endif
#if MAX31865_FILTER_50HZ
    cfg |= CFG_50HZ;
#endif
    return cfg;
}

/* Chuyển điện trở → nhiệt độ */
static float rtd_to_temp(float r)
{
    const float r0 = MAX31865_RNOMINAL;
    float z1 = -CVD_A;
    float z2 = CVD_A * CVD_A - 4.0f * CVD_B;
    float z3 = 4.0f * CVD_B / r0;
    float z4 = 2.0f * CVD_B;

    float t = (sqrtf(z2 + z3 * r) + z1) / z4;
    if (t >= 0.0f) return t;

    /* < 0 °C: xấp xỉ đa thức */
    float rn = r / r0 * 100.0f, rp = rn;
    t = -242.02f;
    t += 2.2228f * rp;     rp *= rn;
    t += 2.5859e-3f * rp;  rp *= rn;
    t -= 4.8260e-6f * rp;  rp *= rn;
    t -= 2.8183e-8f * rp;  rp *= rn;
    t += 1.5243e-10f * rp;
    return t;
}

void MAX31865_Init(void)
{
    cs_high();
    HAL_Delay(10);
    write_reg(REG_CONFIG, base_config() | CFG_FAULTCLR);
    HAL_Delay(70);   /* chờ chuyển đổi đầu tiên */
}

bool MAX31865_Read(max31865_data_t *out)
{
    uint8_t buf[2];
    read_regs(REG_RTD_MSB, buf, 2);
    uint16_t rtd = (uint16_t)((buf[0] << 8) | buf[1]);

    out->fault = 0;
    if (rtd & 0x0001) {                     /* bit lỗi */
        read_regs(REG_FAULT, &out->fault, 1);
        write_reg(REG_CONFIG, base_config() | CFG_FAULTCLR);
        out->ok = false;
        return false;
    }
    if (rtd == 0x0000 || rtd == 0xFFFE) {   /* không có module / SPI lỗi */
        out->fault = 0xFF;
        out->ok = false;
        return false;
    }

    rtd >>= 1;
    out->r_ohm  = (float)rtd * MAX31865_RREF / 32768.0f;
    out->temp_c = rtd_to_temp(out->r_ohm) + TEMP_OFFSET_C;
    out->ok     = (out->temp_c > -50.0f && out->temp_c < 250.0f);
    return out->ok;
}
