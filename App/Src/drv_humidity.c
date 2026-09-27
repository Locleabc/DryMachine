/**
 * @file    drv_humidity.c
 * @note    File này định nghĩa HAL_UART_RxCpltCallback / HAL_UART_ErrorCallback.
 *          Nếu sau này UART khác cần callback, chuyển các hàm này về app.c
 *          và gọi Humidity_UartRxCplt()/Humidity_UartError() từ đó.
 */
#include "drv_humidity.h"
#include "app_config.h"
#include <string.h>

#define RESP_LEN   (5 + 2 * HUM_REG_COUNT)   /* addr+fc+bytecount+data+crc(2) */

typedef enum { HS_IDLE = 0, HS_WAIT, HS_DONE, HS_ERROR } hum_state_t;

static volatile hum_state_t s_state = HS_IDLE;
static uint8_t  s_rx[RESP_LEN];
static uint32_t s_req_tick;
static uint8_t  s_consec_err;
static humidity_data_t s_data;

static uint16_t crc16_modbus(const uint8_t *buf, uint16_t len)
{
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < len; i++) {
        crc ^= buf[i];
        for (int b = 0; b < 8; b++) {
            crc = (crc & 1) ? (uint16_t)((crc >> 1) ^ 0xA001) : (uint16_t)(crc >> 1);
        }
    }
    return crc;
}

static inline void de_tx(void) { HAL_GPIO_WritePin(RS485_DE_GPIO_Port, RS485_DE_Pin, GPIO_PIN_SET); }
static inline void de_rx(void) { HAL_GPIO_WritePin(RS485_DE_GPIO_Port, RS485_DE_Pin, GPIO_PIN_RESET); }

static void on_error(void)
{
    s_data.err_count++;
    if (s_consec_err < 255) s_consec_err++;
    if (s_consec_err >= HUM_MAX_ERRORS) s_data.ok = false;
}

void Humidity_Init(void)
{
    de_rx();
    memset(&s_data, 0, sizeof(s_data));
    s_state = HS_IDLE;
    s_consec_err = 0;
}

void Humidity_Request(void)
{
    if (s_state == HS_WAIT) return;   /* đang chờ phản hồi */

    uint8_t req[8];
    req[0] = HUM_MODBUS_ADDR;
    req[1] = HUM_MODBUS_FUNC;
    req[2] = (uint8_t)(HUM_REG_START >> 8);
    req[3] = (uint8_t)(HUM_REG_START & 0xFF);
    req[4] = 0x00;
    req[5] = HUM_REG_COUNT;
    uint16_t crc = crc16_modbus(req, 6);
    req[6] = (uint8_t)(crc & 0xFF);
    req[7] = (uint8_t)(crc >> 8);

    HAL_UART_AbortReceive(RS485_UART);
    __HAL_UART_CLEAR_OREFLAG(RS485_UART);

    de_tx();
    HAL_UART_Transmit(RS485_UART, req, sizeof(req), 20);   /* chờ đến khi TC = 1 */
    de_rx();

    s_req_tick = HAL_GetTick();
    s_state = HS_WAIT;
    if (HAL_UART_Receive_IT(RS485_UART, s_rx, RESP_LEN) != HAL_OK) {
        s_state = HS_ERROR;
    }
}

void Humidity_Poll(void)
{
    switch (s_state) {
    case HS_WAIT:
        if (HAL_GetTick() - s_req_tick > HUM_TIMEOUT_MS) {
            HAL_UART_AbortReceive(RS485_UART);
            on_error();
            s_state = HS_IDLE;
        }
        break;

    case HS_DONE: {
        uint16_t crc = crc16_modbus(s_rx, RESP_LEN - 2);
        bool valid = (s_rx[0] == HUM_MODBUS_ADDR) &&
                     (s_rx[1] == HUM_MODBUS_FUNC) &&
                     (s_rx[2] == 2 * HUM_REG_COUNT) &&
                     (s_rx[RESP_LEN - 2] == (uint8_t)(crc & 0xFF)) &&
                     (s_rx[RESP_LEN - 1] == (uint8_t)(crc >> 8));
        if (valid) {
            int16_t t  = (int16_t)((s_rx[3 + 2 * HUM_REG_IDX_TEMP] << 8) | s_rx[4 + 2 * HUM_REG_IDX_TEMP]);
            uint16_t h = (uint16_t)((s_rx[3 + 2 * HUM_REG_IDX_HUM] << 8) | s_rx[4 + 2 * HUM_REG_IDX_HUM]);
            s_data.temp_c = (float)t / HUM_SCALE;
            s_data.rh     = (float)h / HUM_SCALE;
            s_data.ok     = (s_data.rh >= 0.0f && s_data.rh <= 100.0f);
            s_consec_err  = 0;
        } else {
            on_error();
        }
        s_state = HS_IDLE;
        break;
    }

    case HS_ERROR:
        on_error();
        s_state = HS_IDLE;
        break;

    default:
        break;
    }
}

void Humidity_Get(humidity_data_t *out)
{
    *out = s_data;
}

/* ---------------- HAL callbacks ---------------- */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart == RS485_UART && s_state == HS_WAIT) s_state = HS_DONE;
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart == RS485_UART && s_state == HS_WAIT) s_state = HS_ERROR;
}
