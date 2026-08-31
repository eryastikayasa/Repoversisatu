#include "display.h"
#include "esp_log.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

static const char *TAG = "FACE_ANIM";
static TaskHandle_t anim_task_handle = NULL;
static SemaphoreHandle_t anim_start_mutex = NULL;

static uint32_t rnd(uint32_t max_value) { return max_value ? esp_random() % max_value : 0; }

static void render(int expr, int step, int sx, int sy, int look)
{
    display_render_mochi(expr, step, sx, sy, look);
}

static void smooth_move(int from_x, int from_y, int to_x, int to_y,
                        uint32_t duration_ms, int look, face_state_t state)
{
    const int step_ms = 30;
    int steps = (int)(duration_ms / step_ms);
    if (steps < 1) steps = 1;
    int expr = 0;
    if (state == FACE_HAPPY || state == FACE_LISTENING) expr = 1;
    else if (state == FACE_SAD) expr = 6;
    else if (state == FACE_ERROR) expr = 99;
    else if (state == FACE_SPEAKING) expr = 2;

    for (int i = 1; i <= steps; ++i) {
        if (face_get_state() != state) return;
        int x = from_x + ((to_x - from_x) * i) / steps;
        int y = from_y + ((to_y - from_y) * i) / steps;
        render(expr, 0, x, y, look);
        vTaskDelay(pdMS_TO_TICKS(step_ms));
    }
}

static void blink(face_state_t state, bool double_blink)
{
    int expr = (state == FACE_HAPPY || state == FACE_LISTENING) ? 2 :
               (state == FACE_SAD ? 6 : 0);
    if (face_get_state() != state) return;
    render(expr, 1, 0, 0, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    if (face_get_state() != state) return;
    render(expr, 0, 0, 0, 0);
    vTaskDelay(pdMS_TO_TICKS(120));
    if (double_blink && face_get_state() == state) {
        render(expr, 1, 0, 0, 0);
        vTaskDelay(pdMS_TO_TICKS(100));
        if (face_get_state() == state) render(expr, 0, 0, 0, 0);
    }
}

static int idle_shift_x = 0;
static int idle_shift_y = 0;
static int idle_expression = 0;
static TickType_t next_shift_tick = 0;
static bool idle_shift_initialized = false;

static void update_idle_shift(void)
{
    TickType_t now = xTaskGetTickCount();
    if (!idle_shift_initialized || now >= next_shift_tick) {
        idle_shift_initialized = true;
        idle_shift_x = (int)rnd(7) - 3;
        idle_shift_y = (int)rnd(5) - 2;
        idle_expression = (int)rnd(3);
        next_shift_tick = now + pdMS_TO_TICKS(30000 + rnd(30001));
    }
}

static void idle_render(int step, int extra_x, int extra_y, int look)
{
    update_idle_shift();
    render(idle_expression, step,
           idle_shift_x + extra_x,
           idle_shift_y + extra_y,
           look);
}

static void idle_sequence(void)
{
    update_idle_shift();
    vTaskDelay(pdMS_TO_TICKS(1500 + rnd(2001)));
    if (face_get_state() != FACE_IDLE) return;
    update_idle_shift();
    uint32_t behavior = rnd(100);

    if (behavior < 30) {
        int x = (rnd(2) == 0) ? -4 : 4;
        int y = (int)rnd(7) - 3;
        smooth_move(idle_shift_x, idle_shift_y,
                    idle_shift_x + x, idle_shift_y + y, 180, 0, FACE_IDLE);
        if (face_get_state() != FACE_IDLE) return;
        vTaskDelay(pdMS_TO_TICKS(180 + rnd(121)));
        smooth_move(idle_shift_x + x, idle_shift_y + y,
                    idle_shift_x, idle_shift_y, 180, 0, FACE_IDLE);
    } else if (behavior < 55) {
        int look = (rnd(2) == 0) ? 1 : 2;
        idle_render(0, 0, 0, look);
        vTaskDelay(pdMS_TO_TICKS(450 + rnd(401)));
        if (face_get_state() == FACE_IDLE) idle_render(0, 0, 0, 0);
    } else if (behavior < 70) {
        int look = (rnd(2) == 0) ? 1 : 2;
        int x = (look == 1) ? -3 : 3;
        int y = (look == 1) ? -2 : 2;
        smooth_move(idle_shift_x, idle_shift_y,
                    idle_shift_x + x, idle_shift_y + y, 150, look, FACE_IDLE);
        if (face_get_state() != FACE_IDLE) return;
        vTaskDelay(pdMS_TO_TICKS(200 + rnd(201)));
        smooth_move(idle_shift_x + x, idle_shift_y + y,
                    idle_shift_x, idle_shift_y, 150, 0, FACE_IDLE);
    } else {
        blink(FACE_IDLE, rnd(5) == 0);
    }
}

static void state_sequence(face_state_t state)
{
    switch (state) {
    case FACE_LISTENING:
        render(1, 0, 0, 0, 0);
        vTaskDelay(pdMS_TO_TICKS(180));
        break;
    case FACE_THINKING:
        smooth_move(0, 0, -4, -2, 180, 1, state);
        if (face_get_state() != state) break;
        vTaskDelay(pdMS_TO_TICKS(120));
        smooth_move(-4, -2, 4, -2, 360, 2, state);
        if (face_get_state() != state) break;
        vTaskDelay(pdMS_TO_TICKS(120));
        smooth_move(4, -2, 0, 0, 180, 0, state);
        break;
    case FACE_SPEAKING:
        while (face_get_state() == FACE_SPEAKING) {
            render(2, 0, 0, 0, 0);
            vTaskDelay(pdMS_TO_TICKS(120));
            if (face_get_state() != FACE_SPEAKING) break;
            render(2, 1, 0, 0, 0);
            vTaskDelay(pdMS_TO_TICKS(90));
            if (face_get_state() != FACE_SPEAKING) break;
            render(2, 0, 0, 0, 1);
            vTaskDelay(pdMS_TO_TICKS(120));
        }
        break;
    case FACE_HAPPY:
        render(2, 0, 0, -1, 0);
        vTaskDelay(pdMS_TO_TICKS(140));
        if (face_get_state() != FACE_HAPPY) break;
        render(2, 0, 0, 0, 0);
        vTaskDelay(pdMS_TO_TICKS(140));
        if (face_get_state() == FACE_HAPPY) face_set_state(FACE_LISTENING);
        break;
    case FACE_SAD:
        smooth_move(0, 0, 0, 2, 120, 0, state);
        if (face_get_state() != state) break;
        render(6, 0, 0, 0, 0);
        vTaskDelay(pdMS_TO_TICKS(250));
        break;
    case FACE_ERROR:
        smooth_move(0, 0, 1, 0, 120, 0, state);
        if (face_get_state() != state) break;
        render(99, 0, 0, 0, 0);
        vTaskDelay(pdMS_TO_TICKS(250));
        break;
    case FACE_SLEEP:
        render(0, 1, 0, 0, 0);
        vTaskDelay(pdMS_TO_TICKS(500));
        break;
    case FACE_IDLE:
    default:
        idle_sequence();
        break;
    }
}

static void face_animation_task(void *arg)
{
    (void)arg;
    oled_init();
    while (1) state_sequence(face_get_state());
}

void face_animation_start(void)
{
    if (!anim_start_mutex) {
        anim_start_mutex = xSemaphoreCreateMutex();
        if (!anim_start_mutex) { ESP_LOGE(TAG, "Gagal membuat mutex animasi OLED"); return; }
    }
    xSemaphoreTake(anim_start_mutex, portMAX_DELAY);
    if (!anim_task_handle) {
        BaseType_t ok = xTaskCreate(face_animation_task, "face_anim", 6144, NULL, 2, &anim_task_handle);
        if (ok != pdPASS) {
            ESP_LOGE(TAG, "Gagal membuat task animasi OLED");
            anim_task_handle = NULL;
        } else {
            ESP_LOGI(TAG, "Mochi OLED animation aktif");
        }
    }
    xSemaphoreGive(anim_start_mutex);
}

void face_animation_stop(void)
{
    if (!anim_start_mutex) return;
    xSemaphoreTake(anim_start_mutex, portMAX_DELAY);
    if (anim_task_handle) { vTaskDelete(anim_task_handle); anim_task_handle = NULL; }
    xSemaphoreGive(anim_start_mutex);
}
