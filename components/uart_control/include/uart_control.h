#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void uart_control_init(void);
void uart_control_send(const char *cmd);
int uart_control_read(char *buf, size_t max_len);

#ifdef __cplusplus
}
#endif
