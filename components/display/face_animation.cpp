#include "display.h"
#include "esp_log.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

static const char *TAG = "FACE_ANIM";
static TaskHandle_t anim_task_handle = NULL;
static SemaphoreHandle_t anim_start_mutex = NULL;

static uint32_t rnd(uint32_t max_value)
{
    return max_value ? esp_random() % max_value : 0;
}

static void render(int expr, int step, int sx, int sy, int look)
{
    display_render_mochi(expr, step, sx, sy, look);
}

static bool blink_at(face_state_t state, int expr, int look)
{
    if (face_get_state() != state) return false;
    render(expr, 1, 0, 0, look);
    vTaskDelay(pdMS_TO_TICKS(65 + rnd(40)));
    if (face_get_state() != state) return false;
    render(expr, 0, 0, 0, look);
    return true;
}

static bool natural_blink(face_state_t state, int expr, int look)
{
    if (!blink_at(state, expr, look)) return false;

    /* Sesekali double blink. */
    if (rnd(5) == 0) {
        vTaskDelay(pdMS_TO_TICKS(100 + rnd(100)));
        if (!blink_at(state, expr, look)) return false;
    }
    return true;
}

static void idle_sequence(void)
{
    vTaskDelay(pdMS_TO_TICKS(1200 + rnd(1800)));
    if (face_get_state() != FACE_IDLE) return;

    uint32_t b = rnd(100);

    if (b < 25) {
        render(0, 0, 0, 0, 1);
        vTaskDelay(pdMS_TO_TICKS(350 + rnd(400)));
        if (face_get_state() != FACE_IDLE) return;
        render(0, 0, 0, 0, 0);
    }
    else if (b < 50) {
        render(0, 0, 0, 0, 2);
        vTaskDelay(pdMS_TO_TICKS(350 + rnd(400)));
        if (face_get_state() != FACE_IDLE) return;
        render(0, 0, 0, 0, 0);
    }
    else if (b < 70) {
        /* Lirik kiri -> tengah -> kanan -> tengah. */
        render(0, 0, 0, 0, 1);
        vTaskDelay(pdMS_TO_TICKS(160 + rnd(120)));
        if (face_get_state() != FACE_IDLE) return;
        render(0, 0, 0, 0, 0);
        vTaskDelay(pdMS_TO_TICKS(120 + rnd(120)));
        if (face_get_state() != FACE_IDLE) return;
        render(0, 0, 0, 0, 2);
        vTaskDelay(pdMS_TO_TICKS(160 + rnd(120)));
        if (face_get_state() != FACE_IDLE) return;
        render(0, 0, 0, 0, 0);
    }
    else {
        natural_blink(FACE_IDLE, 0, 0);
    }
}

static void listening_sequence(void)
{
    render(1, 0, 0, 0, 0);
    vTaskDelay(pdMS_TO_TICKS(650 + rnd(800)));
    if (face_get_state() != FACE_LISTENING) return;

    uint32_t b = rnd(100);

    if (b < 30) {
        render(1, 0, 0, 0, 1);
        vTaskDelay(pdMS_TO_TICKS(450 + rnd(450)));
        if (face_get_state() != FACE_LISTENING) return;
        render(1, 0, 0, 0, 0);
    }
    else if (b < 60) {
        render(1, 0, 0, 0, 2);
        vTaskDelay(pdMS_TO_TICKS(450 + rnd(450)));
        if (face_get_state() != FACE_LISTENING) return;
        render(1, 0, 0, 0, 0);
    }
    else {
        natural_blink(FACE_LISTENING, 1, 0);
    }
}

static void thinking_sequence(void)
{
    render(0, 0, 0, 0, 0);
    vTaskDelay(pdMS_TO_TICKS(220 + rnd(220)));
    if (face_get_state() != FACE_THINKING) return;

    /* Melihat ke atas, pupil ikut naik. */
    render(0, 0, 0, 0, 3);
    vTaskDelay(pdMS_TO_TICKS(500 + rnd(500)));
    if (face_get_state() != FACE_THINKING) return;

    if (rnd(3) != 0) {
        blink_at(FACE_THINKING, 0, 3);
        if (face_get_state() != FACE_THINKING) return;
    }

    vTaskDelay(pdMS_TO_TICKS(350 + rnd(450)));
    if (face_get_state() != FACE_THINKING) return;
    render(0, 0, 0, 0, 0);
}

static void speaking_sequence(void)
{
    /* Mata selalu tengah ketika berbicara. */
    render(2, 0, 0, 0, 0);
    vTaskDelay(pdMS_TO_TICKS(900 + rnd(1300)));
    if (face_get_state() != FACE_SPEAKING) return;
    natural_blink(FACE_SPEAKING, 2, 0);
}

static void happy_sequence(void)
{
    if (face_get_state() != FACE_HAPPY) return;

    /* step 2 = mata tertutup melengkung. */
    render(2, 2, 0, 0, 0);
    vTaskDelay(pdMS_TO_TICKS(650));
    if (face_get_state() != FACE_HAPPY) return;

    render(2, 2, 0, -1, 0);
    vTaskDelay(pdMS_TO_TICKS(250));
    if (face_get_state() == FACE_HAPPY) face_set_state(FACE_LISTENING);
}

static void sad_sequence(void)
{
    if (face_get_state() != FACE_SAD) return;
    render(6, 0, 0, 0, 0);
    vTaskDelay(pdMS_TO_TICKS(650 + rnd(450)));
}

static void error_sequence(void)
{
    if (face_get_state() != FACE_ERROR) return;

    /* X X + shake kecil. */
    render(99, 0, -1, 0, 0);
    vTaskDelay(pdMS_TO_TICKS(65));
    if (face_get_state() != FACE_ERROR) return;

    render(99, 0, 1, 0, 0);
    vTaskDelay(pdMS_TO_TICKS(65));
    if (face_get_state() != FACE_ERROR) return;

    render(99, 0, 0, 0, 0);
    vTaskDelay(pdMS_TO_TICKS(450));
}

static void sleep_sequence(void)
{
    if (face_get_state() != FACE_SLEEP) return;

    /* step 3 = mata tertutup mendatar. */
    render(0, 3, 0, 0, 0);
    vTaskDelay(pdMS_TO_TICKS(1200));
}

static void state_sequence(face_state_t state)
{
    switch (state) {
        case FACE_IDLE:      idle_sequence(); break;
        case FACE_LISTENING: listening_sequence(); break;
        case FACE_THINKING:  thinking_sequence(); break;
        case FACE_SPEAKING:  speaking_sequence(); break;
        case FACE_HAPPY:     happy_sequence(); break;
        case FACE_SAD:       sad_sequence(); break;
        case FACE_ERROR:     error_sequence(); break;
        case FACE_SLEEP:     sleep_sequence(); break;
        default:             idle_sequence(); break;
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
        if (!anim_start_mutex) {
            ESP_LOGE(TAG, "Gagal membuat mutex animasi OLED");
            return;
        }
    }

    xSemaphoreTake(anim_start_mutex, portMAX_DELAY);

    if (!anim_task_handle) {
        BaseType_t ok = xTaskCreate(
            face_animation_task,
            "face_anim",
            6144,
            NULL,
            2,
            &anim_task_handle
        );

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
    if (anim_task_handle) {
        vTaskDelete(anim_task_handle);
        anim_task_handle = NULL;
    }
    xSemaphoreGive(anim_start_mutex);
}
