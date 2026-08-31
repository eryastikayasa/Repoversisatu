#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void uart_control_init(void);
void uart_control_send(const char *cmd);
int uart_control_read(char *buf, size_t max_len);

// Validate and execute one Gemini device command over UART.
bool uart_control_execute_command(const char *command);

// Legacy text-action helper retained for compatibility.
bool uart_control_process_action_text(const char *text);

#ifdef __cplusplus
}
#endif
