#pragma once

#include "driver/gpio.h"
#include <stdint.h>
#include <stdbool.h>

// XiaoZhi bread-compact-wifi-128x64 reference pinout.
#define OLED_SDA_PIN  GPIO_NUM_41
#define OLED_SCL_PIN  GPIO_NUM_42
#define OLED_I2C_ADDR 0x3C
#define OLED_WIDTH    128
#define OLED_HEIGHT   64

typedef enum {
    FACE_IDLE = 0,
    FACE_LISTENING,
    FACE_THINKING,
    FACE_SPEAKING,
    FACE_HAPPY,
    FACE_SAD,
    FACE_ERROR,
    FACE_SLEEP
} face_state_t;

#ifdef __cplusplus
extern "C" {
#endif

void oled_init(void);
void display_status(const char *status);
void face_render(void);
void display_render_buffer(const uint8_t *buffer);

// Mochi renderer: expr 0=normal, 1=smile, 2=happy, 3=wink, 6=sad, 99=IR/self-test.
// step=0 open, step=1 blink. sX/sY shift the whole pair of eyes; arahLirik 0=center, 1=left, 2=right.
void display_render_mochi(int expr, int step, int sX, int sY, int arahLirik);

void face_animation_start(void);
void face_animation_stop(void);
void face_set_state(face_state_t state);
face_state_t face_get_state(void);

#ifdef __cplusplus
}
#endif
