/**
 * @file    sht4x.c
 * @note    Theo datasheet Sensirion SHT4x: lệnh 1 byte, trả 6 byte
 *          [T_msb T_lsb CRC RH_msb RH_lsb CRC], CRC-8 poly 0x31 init 0xFF.
 */
#include "sht4x.h"

#define CMD_SOFT_RESET   0x94
#define CMD_SERIAL       0x89
#define I2C_TIMEOUT      10

static uint8_t crc8(const uint8_t *d, uint8_t len)
{
    uint8_t crc = 0xFF;
    while (len--) {
        crc ^= *d++;
        for (int i = 0; i < 8; i++) crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x31) : (uint8_t)(crc << 1);
    }
    return crc;
}

static sht4x_status_t hal_err(const sht4x_t *dev, HAL_StatusTypeDef r)
{
    if (r == HAL_OK)   return SHT4X_OK;
    if (r == HAL_BUSY) return SHT4X_ERR_BUSY;
    if (dev->cfg->hi2c->ErrorCode & HAL_I2C_ERROR_AF) return SHT4X_ERR_NACK;
    return SHT4X_ERR_BUS;
}

static sht4x_status_t send_cmd(sht4x_t *dev, uint8_t cmd)
{
    return hal_err(dev, HAL_I2C_Master_Transmit(dev->cfg->hi2c, (uint16_t)(dev->addr << 1), &cmd, 1, I2C_TIMEOUT));
}

static sht4x_status_t read6(sht4x_t *dev, uint8_t buf[6])
{
    sht4x_status_t st = hal_err(dev, HAL_I2C_Master_Receive(dev->cfg->hi2c, (uint16_t)(dev->addr << 1), buf, 6, I2C_TIMEOUT));
    if (st != SHT4X_OK) return st;
    if (crc8(&buf[0], 2) != buf[2] || crc8(&buf[3], 2) != buf[5])
        return SHT4X_ERR_CRC;
    return SHT4X_OK;
}

static sht4x_status_t init_at(sht4x_t *dev, uint8_t addr)
{
    uint8_t buf[6];
    dev->addr = addr;
    sht4x_status_t st = send_cmd(dev, CMD_SOFT_RESET);
    if (st != SHT4X_OK) return st;
    HAL_Delay(2);                                   /* soft reset ≤ 1 ms */

    st = send_cmd(dev, CMD_SERIAL);
    if (st != SHT4X_OK) return st;
    HAL_Delay(2);
    st = read6(dev, buf);
    if (st != SHT4X_OK) return st;
    dev->serial = ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) | ((uint32_t)buf[3] << 8) | buf[4];
    return SHT4X_OK;
}

sht4x_status_t SHT4X_Init(sht4x_t *dev, const sht4x_cfg_t *cfg)
{
    dev->cfg = cfg;
    dev->serial = 0;
    dev->busy_ms = 0;
    sht4x_status_t st = init_at(dev, cfg->addr);
    if (st == SHT4X_ERR_NACK) {                     /* không trả lời → thử địa chỉ còn lại */
        uint8_t other = (cfg->addr == SHT4X_ADDR_A) ? SHT4X_ADDR_B : SHT4X_ADDR_A;
        if (init_at(dev, other) == SHT4X_OK) return SHT4X_OK;
        dev->addr = cfg->addr;
    }
    return st;
}

sht4x_status_t SHT4X_StartMeasure(sht4x_t *dev)
{
    switch (dev->cfg->precision) {
    case SHT4X_PREC_LOW:    dev->busy_ms = 2;  break;
    case SHT4X_PREC_MEDIUM: dev->busy_ms = 5;  break;
    default:                dev->busy_ms = 10; break;
    }
    return send_cmd(dev, (uint8_t)dev->cfg->precision);
}

sht4x_status_t SHT4X_StartHeater(sht4x_t *dev, sht4x_heater_t heat)
{
    bool long_pulse = (heat == SHT4X_HEAT_200MW_1S || heat == SHT4X_HEAT_110MW_1S || heat == SHT4X_HEAT_20MW_1S);
    dev->busy_ms = long_pulse ? 1100 : 110;
    return send_cmd(dev, (uint8_t)heat);
}

uint32_t SHT4X_BusyTimeMs(const sht4x_t *dev)
{
    return dev->busy_ms;
}

sht4x_status_t SHT4X_ReadResult(sht4x_t *dev, sht4x_result_t *out)
{
    uint8_t buf[6];
    sht4x_status_t st = read6(dev, buf);
    if (st != SHT4X_OK) return st;

    uint16_t t_raw  = (uint16_t)((buf[0] << 8) | buf[1]);
    uint16_t rh_raw = (uint16_t)((buf[3] << 8) | buf[4]);
    out->temp_c = -45.0f + 175.0f * (float)t_raw / 65535.0f;
    float rh    =  -6.0f + 125.0f * (float)rh_raw / 65535.0f;
    if (rh < 0.0f)   rh = 0.0f;
    if (rh > 100.0f) rh = 100.0f;
    out->rh = rh;
    return SHT4X_OK;
}
