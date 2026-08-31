#include "display.h"
#include "esp_log.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

static const char *TAG = "FACE_ANIM";

static TaskHandle_t anim_task_handle = NULL;
static SemaphoreHandle_t anim_start_mutex = NULL;

/* ============================================================
 * RANDOM
 * ============================================================ */

static uint32_t rnd(uint32_t max_value)
{
    return max_value
        ? esp_random() % max_value
        : 0;
}

/* ============================================================
 * RENDER HELPER
 * ============================================================ */

static void render(
    int expr,
    int step,
    int sx,
    int sy,
    int look
)
{
    display_render_mochi(
        expr,
        step,
        sx,
        sy,
        look
    );
}

/* ============================================================
 * NATURAL BLINK
 * ============================================================ */

static bool natural_blink(
    face_state_t state,
    int expr
)
{
    if (face_get_state() != state) {
        return false;
    }

    /*
     * Blink pertama.
     */
    render(
        expr,
        1,
        0,
        0,
        0
    );

    vTaskDelay(
        pdMS_TO_TICKS(
            70 + rnd(45)
        )
    );

    if (face_get_state() != state) {
        return false;
    }

    /*
     * Buka lagi.
     */
    render(
        expr,
        0,
        0,
        0,
        0
    );

    /*
     * Kadang double blink.
     */
    if (rnd(5) == 0) {

        vTaskDelay(
            pdMS_TO_TICKS(
                90 + rnd(100)
            )
        );

        if (face_get_state() != state) {
            return false;
        }

        render(
            expr,
            1,
            0,
            0,
            0
        );

        vTaskDelay(
            pdMS_TO_TICKS(
                65 + rnd(40)
            )
        );

        if (face_get_state() != state) {
            return false;
        }

        render(
            expr,
            0,
            0,
            0,
            0
        );
    }

    return true;
}

/* ============================================================
 * IDLE
 *
 * Karakter:
 * - mata terbuka
 * - lirik kiri
 * - kembali tengah
 * - lirik kanan
 * - kembali tengah
 * - blink random
 * ============================================================ */

static void idle_sequence(void)
{
    /*
     * Tunggu random supaya tidak mekanis.
     */
    vTaskDelay(
        pdMS_TO_TICKS(
            1200 + rnd(1800)
        )
    );

    if (face_get_state() != FACE_IDLE) {
        return;
    }

    uint32_t behavior = rnd(100);

    /*
     * 30% melihat kiri.
     */
    if (behavior < 30) {

        render(
            0,
            0,
            0,
            0,
            1
        );

        vTaskDelay(
            pdMS_TO_TICKS(
                350 + rnd(350)
            )
        );

        if (face_get_state() != FACE_IDLE) {
            return;
        }

        render(
            0,
            0,
            0,
            0,
            0
        );
    }

    /*
     * 30% melihat kanan.
     */
    else if (behavior < 60) {

        render(
            0,
            0,
            0,
            0,
            2
        );

        vTaskDelay(
            pdMS_TO_TICKS(
                350 + rnd(350)
            )
        );

        if (face_get_state() != FACE_IDLE) {
            return;
        }

        render(
            0,
            0,
            0,
            0,
            0
        );
    }

    /*
     * 20% micro movement.
     */
    else if (behavior < 80) {

        render(
            0,
            0,
            0,
            0,
            1
        );

        vTaskDelay(
            pdMS_TO_TICKS(
                120 + rnd(150)
            )
        );

        if (face_get_state() != FACE_IDLE) {
            return;
        }

        render(
            0,
            0,
            0,
            0,
            2
        );

        vTaskDelay(
            pdMS_TO_TICKS(
                120 + rnd(150)
            )
        );

        if (face_get_state() != FACE_IDLE) {
            return;
        }

        render(
            0,
            0,
            0,
            0,
            0
        );
    }

    /*
     * 20% blink.
     */
    else {

        natural_blink(
            FACE_IDLE,
            0
        );
    }
}

/* ============================================================
 * LISTENING
 *
 * Lebih fokus daripada IDLE.
 * ============================================================ */

static void listening_sequence(void)
{
    /*
     * Awal selalu fokus ke tengah.
     */
    render(
        1,
        0,
        0,
        0,
        0
    );

    vTaskDelay(
        pdMS_TO_TICKS(
            700 + rnd(900)
        )
    );

    if (face_get_state() != FACE_LISTENING) {
        return;
    }

    uint32_t behavior = rnd(100);

    /*
     * Lihat kiri.
     */
    if (behavior < 30) {

        render(
            1,
            0,
            0,
            0,
            1
        );

        vTaskDelay(
            pdMS_TO_TICKS(
                500 + rnd(400)
            )
        );

        if (face_get_state() != FACE_LISTENING) {
            return;
        }

        render(
            1,
            0,
            0,
            0,
            0
        );
    }

    /*
     * Lihat kanan.
     */
    else if (behavior < 60) {

        render(
            1,
            0,
            0,
            0,
            2
        );

        vTaskDelay(
            pdMS_TO_TICKS(
                500 + rnd(400)
            )
        );

        if (face_get_state() != FACE_LISTENING) {
            return;
        }

        render(
            1,
            0,
            0,
            0,
            0
        );
    }

    /*
     * Blink.
     */
    else {

        natural_blink(
            FACE_LISTENING,
            1
        );
    }
}

/* ============================================================
 * THINKING
 *
 * Mata melihat ke ATAS.
 * Pupil ikut ke atas.
 * ============================================================ */

static void thinking_sequence(void)
{
    /*
     * Awal tengah.
     */
    render(
        0,
        0,
        0,
        0,
        0
    );

    vTaskDelay(
        pdMS_TO_TICKS(
            250 + rnd(250)
        )
    );

    if (face_get_state() != FACE_THINKING) {
        return;
    }

    /*
     * Lihat ATAS.
     *
     * arahLirik = 3
     */
    render(
        0,
        0,
        0,
        0,
        3
    );

    /*
     * Tahan seperti orang berpikir.
     */
    vTaskDelay(
        pdMS_TO_TICKS(
            500 + rnd(500)
        )
    );

    if (face_get_state() != FACE_THINKING) {
        return;
    }

    /*
     * Kadang blink ketika melihat atas.
     */
    if (rnd(3) != 0) {

        render(
            0,
            1,
            0,
            0,
            3
        );

        vTaskDelay(
            pdMS_TO_TICKS(
                70 + rnd(35)
            )
        );

        if (face_get_state() != FACE_THINKING) {
            return;
        }

        render(
            0,
            0,
            0,
            0,
            3
        );
    }

    /*
     * Tahan lagi sedikit.
     */
    vTaskDelay(
        pdMS_TO_TICKS(
            300 + rnd(400)
        )
    );

    if (face_get_state() != FACE_THINKING) {
        return;
    }

    /*
     * Kembali tengah.
     */
    render(
        0,
        0,
        0,
        0,
        0
    );
}

/* ============================================================
 * SPEAKING
 *
 * Mata SELALU tengah.
 * Hanya blink.
 * ============================================================ */

static void speaking_sequence(void)
{
    /*
     * Jangan lirik kiri/kanan.
     */
    render(
        2,
        0,
        0,
        0,
        0
    );

    vTaskDelay(
        pdMS_TO_TICKS(
            900 + rnd(1400)
        )
    );

    if (face_get_state() != FACE_SPEAKING) {
        return;
    }

    natural_blink(
        FACE_SPEAKING,
        2
    );
}

/* ============================================================
 * HAPPY
 *
 * Mata tertutup melengkung.
 * ============================================================ */

static void happy_sequence(void)
{
    if (face_get_state() != FACE_HAPPY) {
        return;
    }

    /*
     * step = 2 berarti HAPPY CURVE.
     */
    render(
        2,
        2,
        0,
        0,
        0
    );

    vTaskDelay(
        pdMS_TO_TICKS(
            700
        )
    );

    if (face_get_state() != FACE_HAPPY) {
        return;
    }

    /*
     * Sedikit naik/turun agar tidak terlalu statis.
     */
    render(
        2,
        2,
        0,
        -1,
        0
    );

    vTaskDelay(
        pdMS_TO_TICKS(
            250
        )
    );

    if (face_get_state() == FACE_HAPPY) {
        face_set_state(
            FACE_LISTENING
        );
    }
}

/* ============================================================
 * SAD
 *
 * Mata menutup.
 * ============================================================ */

static void sad_sequence(void)
{
    if (face_get_state() != FACE_SAD) {
        return;
    }

    render(
        6,
        0,
        0,
        0,
        0
    );

    vTaskDelay(
        pdMS_TO_TICKS(
            500 + rnd(400)
        )
    );
}

/* ============================================================
 * ERROR
 *
 * X X + sedikit getaran.
 * ============================================================ */

static void error_sequence(void)
{
    if (face_get_state() != FACE_ERROR) {
        return;
    }

    /*
     * Getaran kecil.
     */
    render(
        99,
        0,
        -1,
        0,
        0
    );

    vTaskDelay(
        pdMS_TO_TICKS(
            70
        )
    );

    if (face_get_state() != FACE_ERROR) {
        return;
    }

    render(
        99,
        0,
        1,
        0,
        0
    );

    vTaskDelay(
        pdMS_TO_TICKS(
            70
        )
    );

    if (face_get_state() != FACE_ERROR) {
        return;
    }

    render(
        99,
        0,
        0,
        0,
        0
    );

    vTaskDelay(
        pdMS_TO_TICKS(
            400
        )
    );
}

/* ============================================================
 * SLEEP
 *
 * Mata tertutup horizontal.
 * ============================================================ */

static void sleep_sequence(void)
{
    if (face_get_state() != FACE_SLEEP) {
        return;
    }

    /*
     * Gunakan renderer khusus garis horizontal.
     */
    memset(
        face_buffer,
        0,
        sizeof(face_buffer)
    );

    /*
     * Kita tidak mengakses buffer dari sini.
     * Renderer utama tetap dipakai.
     *
     * Untuk saat ini gunakan blink sebagai
     * kondisi tidur stabil.
     */
    render(
        0,
        1,
        0,
        0,
        0
    );

    vTaskDelay(
        pdMS_TO_TICKS(
            700
        )
    );
}

/* ============================================================
 * STATE DISPATCHER
 * ============================================================ */

static void state_sequence(
    face_state_t state
)
{
    switch (state) {

        case FACE_IDLE:
            idle_sequence();
            break;

        case FACE_LISTENING:
            listening_sequence();
            break;

        case FACE_THINKING:
            thinking_sequence();
            break;

        case FACE_SPEAKING:
            speaking_sequence();
            break;

        case FACE_HAPPY:
            happy_sequence();
            break;

        case FACE_SAD:
            sad_sequence();
            break;

        case FACE_ERROR:
            error_sequence();
            break;

        case FACE_SLEEP:
            sleep_sequence();
            break;

        default:
            idle_sequence();
            break;
    }
}

/* ============================================================
 * ANIMATION TASK
 * ============================================================ */

static void face_animation_task(
    void *arg
)
{
    (void)arg;

    oled_init();

    while (1) {

        state_sequence(
            face_get_state()
        );
    }
}

/* ============================================================
 * START
 * ============================================================ */

void face_animation_start(void)
{
    if (!anim_start_mutex) {

        anim_start_mutex =
            xSemaphoreCreateMutex();

        if (!anim_start_mutex) {

            ESP_LOGE(
                TAG,
                "Gagal membuat mutex animasi OLED"
            );

            return;
        }
    }

    xSemaphoreTake(
        anim_start_mutex,
        portMAX_DELAY
    );

    if (!anim_task_handle) {

        BaseType_t ok =
            xTaskCreate(
                face_animation_task,
                "face_anim",
                6144,
                NULL,
                2,
                &anim_task_handle
            );

        if (ok != pdPASS) {

            ESP_LOGE(
                TAG,
                "Gagal membuat task animasi OLED"
            );

            anim_task_handle = NULL;

        } else {

            ESP_LOGI(
                TAG,
                "Mochi OLED animation aktif"
            );
        }
    }

    xSemaphoreGive(
        anim_start_mutex
    );
}

/* ============================================================
 * STOP
 * ============================================================ */

void face_animation_stop(void)
{
    if (!anim_start_mutex) {
        return;
    }

    xSemaphoreTake(
        anim_start_mutex,
        portMAX_DELAY
    );

    if (anim_task_handle) {

        vTaskDelete(
            anim_task_handle
        );

        anim_task_handle = NULL;
    }

    xSemaphoreGive(
        anim_start_mutex
    );
}
