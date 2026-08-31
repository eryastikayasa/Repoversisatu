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

static bool is_valid_action(const char *action)
{
    static const char *const allowed[] = {
        "r1_on", "r1_off", "r2_on", "r2_off", "r3_on", "r3_off", "r4_on", "r4_off",
        "fan_on", "fan_off", "fan_pwr", "fan_speed", "fan_swing", "fan_mode",
        "mp3_mode", "mp3_play", "mp3_eq",
        "m_led", "m_mute", "m_musik", "m_cek",
        "cek_suhu", "cek_cahaya"
    };
    for (size_t i = 0; i < sizeof(allowed) / sizeof(allowed[0]); ++i) {
        if (strcmp(action, allowed[i]) == 0) return true;
    }
    return false;
}

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

bool uart_control_execute_command(const char *command)
{
    if (command == NULL || command[0] == '\0') return false;
    if (!is_valid_action(command)) {
        ESP_LOGW(TAG, "Command ditolak: %s", command);
        return false;
    }
    uart_control_send(command);
    ESP_LOGI(TAG, "Gemini command diterima: %s", command);
    return true;
}

bool uart_control_process_action_text(const char *text)
{
    if (text == NULL) return false;

    const char *tag = strstr(text, "[ACTION:");
    if (tag == NULL) return false;
    tag += 8;

    const char *end = strchr(tag, ']');
    if (end == NULL) return false;

    size_t len = (size_t)(end - tag);
    if (len == 0 || len >= 32) return false;

    char action[32];
    memcpy(action, tag, len);
    action[len] = '\0';
    return uart_control_execute_command(action);
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
