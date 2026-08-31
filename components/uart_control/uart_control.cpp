#include "uart_control.h"
#include "driver/uart.h"
#include "hal/gpio_types.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "UART_CTRL";

#define UART_CONTROL_NUM      UART_NUM_1
#define UART_CONTROL_TX_PIN   GPIO_NUM_17
#define UART_CONTROL_RX_PIN   GPIO_NUM_18
#define UART_CONTROL_BAUDRATE 115200
#define UART_CONTROL_BUF_SIZE 1024

void uart_control_init(void)
{
    uart_config_t uart_cfg = {};
    uart_cfg.baud_rate = UART_CONTROL_BAUDRATE;
    uart_cfg.data_bits = UART_DATA_8_BITS;
    uart_cfg.parity = UART_PARITY_DISABLE;
    uart_cfg.stop_bits = UART_STOP_BITS_1;
    uart_cfg.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    uart_cfg.source_clk = UART_SCLK_DEFAULT;

    ESP_ERROR_CHECK(uart_driver_install(UART_CONTROL_NUM, UART_CONTROL_BUF_SIZE, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_CONTROL_NUM, &uart_cfg));
    ESP_ERROR_CHECK(uart_set_pin(UART_CONTROL_NUM, UART_CONTROL_TX_PIN, UART_CONTROL_RX_PIN,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_LOGI(TAG, "UART control siap: TX=%d RX=%d baud=%d",
             UART_CONTROL_TX_PIN, UART_CONTROL_RX_PIN, UART_CONTROL_BAUDRATE);
}

void uart_control_send(const char *cmd)
{
    if (cmd == NULL || cmd[0] == '\0') return;
    uart_write_bytes(UART_CONTROL_NUM, cmd, strlen(cmd));
    uart_write_bytes(UART_CONTROL_NUM, "\n", 1);
    ESP_LOGI(TAG, "TX: %s", cmd);
}

int uart_control_read(char *buf, size_t max_len)
{
    if (buf == NULL || max_len == 0) return -1;

    size_t idx = 0;
    uint8_t byte;
    while (idx < max_len - 1) {
        int n = uart_read_bytes(UART_CONTROL_NUM, &byte, 1, 0);
        if (n <= 0) break;
        if (byte == '\r') continue;
        if (byte == '\n') {
            buf[idx] = '\0';
            return (int)idx;
        }
        buf[idx++] = (char)byte;
    }

    buf[idx] = '\0';
    return idx > 0 ? (int)idx : -1;
}
