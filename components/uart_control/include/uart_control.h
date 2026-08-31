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

// Extract and execute a validated [ACTION:xxx] tag from Gemini text.
// Returns true when a valid action was found and sent over UART.
bool uart_control_process_action_text(const char *text);

#ifdef __cplusplus
}
#endif
