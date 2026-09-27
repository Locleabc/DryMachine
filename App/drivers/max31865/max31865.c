/**
 * @file    max31865.c
 */
#include "max31865.h"
#include <math.h>

#define REG_CONFIG      0x00
#define REG_RTD_MSB     0x01
#define REG_FAULT       0x07
#define WRITE_FLAG      0x80

#define CFG_VBIAS       0x80
#define CFG_AUTO        0x40
#define CFG_3WIRE       0x10
#define CFG_FAULTCLR    0x02
#define CFG_50HZ        0x01

#define CVD_A           3.9083e-3f
#define CVD_B          -5.775e-7f
#define SPI_TIMEOUT     10

static void cs(const max31865_cfg_t *c, bool active)
{
    HAL_GPIO_WritePin(c->cs_port, c->cs_pin, active ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

static void write_reg(const max31865_cfg_t *c, uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = { (uint8_t)(reg | WRITE_FLAG), val };
    cs(c, true);
    HAL_SPI_Transmit(c->hspi, buf, 2, SPI_TIMEOUT);
    cs(c, false);
}

static void read_regs(const max31865_cfg_t *c, uint8_t reg, uint8_t *dst, uint8_t len)
{
    uint8_t addr = reg & 0x7F;
    cs(c, true);
    HAL_SPI_Transmit(c->hspi, &addr, 1, SPI_TIMEOUT);
    HAL_SPI_Receive(c->hspi, dst, len, SPI_TIMEOUT);
    cs(c, false);
}

static uint8_t base_config(const max31865_cfg_t *c)
{
    uint8_t v = CFG_VBIAS | CFG_AUTO;
    if (c->wires == 3) v |= CFG_3WIRE;
    if (c->filter_50hz) v |= CFG_50HZ;
    return v;
}

float MAX31865_ResistanceToTemp(float r, float r0)
{
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

void MAX31865_Init(max31865_t *dev, const max31865_cfg_t *cfg)
{
    dev->cfg = cfg;
    cs(cfg, false);
    HAL_Delay(10);
    write_reg(cfg, REG_CONFIG, base_config(cfg) | CFG_FAULTCLR);
    HAL_Delay(70);              /* chờ chuyển đổi đầu tiên */
}

bool MAX31865_Read(max31865_t *dev, max31865_result_t *out)
{
    const max31865_cfg_t *c = dev->cfg;
    uint8_t buf[2];
    read_regs(c, REG_RTD_MSB, buf, 2);
    uint16_t rtd = (uint16_t)((buf[0] << 8) | buf[1]);

    out->ok = false;
    out->fault = 0;
    if (rtd & 0x0001) {                       /* bit lỗi */
        read_regs(c, REG_FAULT, &out->fault, 1);
        write_reg(c, REG_CONFIG, base_config(c) | CFG_FAULTCLR);
        return false;
    }
    if (rtd == 0x0000 || rtd == 0xFFFE) {     /* không có chip / SPI lỗi */
        out->fault = 0xFF;
        return false;
    }

    rtd >>= 1;
    out->r_ohm  = (float)rtd * c->rref / 32768.0f;
    out->temp_c = MAX31865_ResistanceToTemp(out->r_ohm, c->r0);
    out->ok     = (out->temp_c > -50.0f && out->temp_c < 250.0f);
    return out->ok;
}
