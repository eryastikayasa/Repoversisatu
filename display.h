#pragma once
#include "driver/gpio.h"

#define OLED_SDA_PIN GPIO_NUM_1
#define OLED_SCL_PIN GPIO_NUM_2

void oled_init(void);
void display_status(const char* text);
