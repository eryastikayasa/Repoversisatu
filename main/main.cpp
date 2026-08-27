#include "display.h"
#include "wifi_manager.h"
#include "websocket_mgr.h"
#include "audio_hal.h"
#include "websocket_internal.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "esp_psram.h"
#include "esp_heap_caps.h"
#include "esp_wn_iface.h"
#include "esp_wn_models.h"
#include "model_path.h"
#include "driver/gpio.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "nvs_flash.h"
#include "esp_sntp.h"

#include <sys/time.h>
#include <time.h>

#include <string.h>
#include <errno.h>
#include <netdb.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

static const char *TAG = "MAIN";
#define BOOT_BUTTON_GPIO GPIO_NUM_0
#define WAKE_MODEL_NAME "wn9_hiesp"

// ============================================================
// WAKE WORD - ESP-SR WakeNet9 "Hi, ESP"
// Model is loaded from the ESP-SR "model" partition.
// ESP-SR's CMake generates build/srmodels/srmodels.bin from
// CONFIG_SR_WN_WN9_HIESP and flashes it to that partition.
// ============================================================

static srmodel_list_t *sr_models = nullptr;
static const esp_wn_iface_t *wake_iface = nullptr;
static model_iface_data_t *wake_model = nullptr;
static int wake_chunk_samples = 0;

static bool wakeword_init(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "ESP-SR WAKE WORD INIT");
    ESP_LOGI(TAG, "Model: %s", WAKE_MODEL_NAME);
    ESP_LOGI(TAG, "Loading ESP-SR models from partition: model");

    // IMPORTANT: esp_wn_handle_from_name() must be called only after
    // esp_srmodel_init() has loaded the model list from flash.
    sr_models = esp_srmodel_init("model");
    if (!sr_models) {
        ESP_LOGE(TAG, "ESP-SR model loader gagal: partition 'model' tidak tersedia atau model image tidak valid");
        return false;
    }

    ESP_LOGI(TAG, "ESP-SR models loaded: count=%d", sr_models->num);

    if (esp_srmodel_exists(sr_models, (char *)WAKE_MODEL_NAME) < 0) {
        ESP_LOGE(TAG, "WakeNet model tidak ditemukan di srmodels.bin: %s", WAKE_MODEL_NAME);
        esp_srmodel_deinit(sr_models);
        sr_models = nullptr;
        return false;
    }

    wake_iface = esp_wn_handle_from_name(WAKE_MODEL_NAME);
    if (!wake_iface) {
        ESP_LOGE(TAG, "WakeNet handle tidak ditemukan: %s", WAKE_MODEL_NAME);
        esp_srmodel_deinit(sr_models);
        sr_models = nullptr;
        return false;
    }

    wake_model = wake_iface->create(WAKE_MODEL_NAME, DET_MODE_90);
    if (!wake_model) {
        ESP_LOGE(TAG, "Gagal membuat WakeNet model: %s", WAKE_MODEL_NAME);
        wake_iface = nullptr;
        esp_srmodel_deinit(sr_models);
        sr_models = nullptr;
        return false;
    }

    wake_chunk_samples = wake_iface->get_samp_chunksize(wake_model);
    int wake_rate = wake_iface->get_samp_rate(wake_model);
    int wake_channels = wake_iface->get_channel_num(wake_model);

    ESP_LOGI(TAG, "WakeNet ready: rate=%d Hz chunk=%d samples channels=%d",
             wake_rate, wake_chunk_samples, wake_channels);

    if (wake_rate != MIC_SAMPLE_RATE || wake_channels != 1) {
        ESP_LOGE(TAG, "WakeNet audio mismatch: expected %d Hz mono", MIC_SAMPLE_RATE);
        wake_iface->destroy(wake_model);
        wake_model = nullptr;
        wake_iface = nullptr;
        wake_chunk_samples = 0;
        esp_srmodel_deinit(sr_models);
        sr_models = nullptr;
        return false;
    }

    ESP_LOGI(TAG, "Wake word aktif: HI, ESP");
    ESP_LOGI(TAG, "========================================"); 
    return true;
}

static void wakeword_deinit(void)
{
    if (wake_iface && wake_model) {
        wake_iface->destroy(wake_model);
    }
    wake_model = nullptr;
    wake_iface = nullptr;
    wake_chunk_samples = 0;

    if (sr_models) {
        esp_srmodel_deinit(sr_models);
        sr_models = nullptr;
    }
}

// ============================================================
// NETWORK DEBUG
// ============================================================

static bool debug_dns_resolution(void)
{
    const char *host = "generativelanguage.googleapis.com";
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "DEBUG NETWORK START");
    ESP_LOGI(TAG, "DNS test: %s", host);
    struct addrinfo hints = {};
    struct addrinfo *result = nullptr;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    int err = getaddrinfo(host, "443", &hints, &result);
    if (err != 0) {
        ESP_LOGE(TAG, "DNS FAILED: getaddrinfo error=%d errno=%d", err, errno);
        ESP_LOGI(TAG, "========================================");
        return false;
    }
    ESP_LOGI(TAG, "DNS OK");
    bool found_ipv4 = false;
    for (struct addrinfo *p = result; p != nullptr; p = p->ai_next) {
        if (p->ai_family != AF_INET) continue;
        struct sockaddr_in *addr = (struct sockaddr_in *)p->ai_addr;
        char ip[INET_ADDRSTRLEN] = {};
        if (inet_ntop(AF_INET, &addr->sin_addr, ip, sizeof(ip)) != nullptr)
            ESP_LOGI(TAG, "DNS IPv4: %s", ip);
        found_ipv4 = true;
        break;
    }
    freeaddrinfo(result);
    if (!found_ipv4) {
        ESP_LOGE(TAG, "DNS OK tetapi tidak mendapatkan IPv4");
        ESP_LOGI(TAG, "========================================");
        return false;
    }
    ESP_LOGI(TAG, "DNS RESULT: OK");
    ESP_LOGI(TAG, "========================================");
    return true;
}

static bool debug_tcp_connection(void)
{
    const char *host = "generativelanguage.googleapis.com";
    const char *port = "443";
    ESP_LOGI(TAG, "TCP test: %s:%s", host, port);
    struct addrinfo hints = {};
    struct addrinfo *result = nullptr;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    int err = getaddrinfo(host, port, &hints, &result);
    if (err != 0 || result == nullptr) {
        ESP_LOGE(TAG, "TCP test gagal mendapatkan address: error=%d errno=%d", err, errno);
        return false;
    }
    int sock = -1;
    bool connected = false;
    for (struct addrinfo *p = result; p != nullptr; p = p->ai_next) {
        if (p->ai_family != AF_INET) continue;
        struct sockaddr_in *addr = (struct sockaddr_in *)p->ai_addr;
        char ip[INET_ADDRSTRLEN] = {};
        if (inet_ntop(AF_INET, &addr->sin_addr, ip, sizeof(ip)) != nullptr)
            ESP_LOGI(TAG, "TCP target: %s:443", ip);
        sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
        if (sock < 0) {
            ESP_LOGE(TAG, "TCP socket() FAILED errno=%d", errno);
            continue;
        }
        struct timeval timeout = {};
        timeout.tv_sec = 5;
        timeout.tv_usec = 0;
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        ESP_LOGI(TAG, "TCP connect()...");
        if (connect(sock, p->ai_addr, p->ai_addrlen) == 0) {
            ESP_LOGI(TAG, "TCP CONNECT OK");
            connected = true;
            close(sock);
            break;
        }
        ESP_LOGE(TAG, "TCP CONNECT FAILED errno=%d", errno);
        close(sock);
        sock = -1;
    }
    freeaddrinfo(result);
    if (connected) {
        ESP_LOGI(TAG, "TCP RESULT: OK");
        return true;
    }
    ESP_LOGE(TAG, "TCP RESULT: GAGAL");
    return false;
}

static void debug_network_path(void)
{
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "NETWORK DIAGNOSTIC");
    ESP_LOGI(TAG, "Target: generativelanguage.googleapis.com:443");
    ESP_LOGI(TAG, "========================================");
    if (!debug_dns_resolution()) {
        ESP_LOGE(TAG, "NETWORK STOP: DNS");
        ESP_LOGI(TAG, "========================================");
        return;
    }
    if (!debug_tcp_connection()) {
        ESP_LOGE(TAG, "NETWORK STOP: TCP");
        ESP_LOGI(TAG, "DNS = OK");
        ESP_LOGI(TAG, "TCP = FAILED");
        ESP_LOGI(TAG, "TLS = BELUM DITES");
        ESP_LOGI(TAG, "========================================");
        return;
    }
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "NETWORK BASIC TEST = OK");
    ESP_LOGI(TAG, "DNS = OK");
    ESP_LOGI(TAG, "TCP 443 = OK");
    ESP_LOGI(TAG, "NEXT = WebSocket/TLS");
    ESP_LOGI(TAG, "========================================");
}

// ============================================================
// SNTP
// ============================================================

static void sync_sntp_time(void)
{
    ESP_LOGI(TAG, "Mencari server NTP...");
    display_status("Sync Jam Network..");
    esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "time.google.com");
    esp_sntp_setservername(1, "id.pool.ntp.org");
    esp_sntp_setservername(2, "pool.ntp.org");
    esp_sntp_init();
    int retry = 0;
    const int max_retries = 10;
    time_t now = 0;
    struct tm timeinfo = {};
    while (retry < max_retries) {
        time(&now);
        localtime_r(&now, &timeinfo);
        if (timeinfo.tm_year >= (2024 - 1900)) {
            ESP_LOGI(TAG, "Waktu cocok! Tahun: %d", timeinfo.tm_year + 1900);
            display_status("Jam Cocok!");
            vTaskDelay(pdMS_TO_TICKS(1000));
            return;
        }
        vTaskDelay(pdMS_TO_TICKS(500));
        retry++;
    }
    ESP_LOGW(TAG, "NTP gagal. Menggunakan waktu fallback.");
    struct timeval tv = { .tv_sec = 1770000000, .tv_usec = 0 };
    settimeofday(&tv, NULL);
    display_status("Jam Set Fallback");
    vTaskDelay(pdMS_TO_TICKS(1000));
}

// ============================================================
// MIC ACTIVITY DETECTION
// ============================================================

static bool mic_frame_has_activity(const uint8_t *data, size_t len)
{
    if (!data || len < 2) return false;

    constexpr int32_t SILENCE_THRESHOLD = 200;
    constexpr size_t MIN_ACTIVE_SAMPLES = 8;
    size_t active_samples = 0;

    for (size_t i = 0; i + 1 < len; i += 2) {
        int16_t sample = (int16_t)((uint16_t)data[i] | ((uint16_t)data[i + 1] << 8));
        int32_t magnitude = sample < 0 ? -(int32_t)sample : (int32_t)sample;
        if (magnitude >= SILENCE_THRESHOLD) {
            active_samples++;
            if (active_samples >= MIN_ACTIVE_SAMPLES) return true;
        }
    }
    return false;
}

// ============================================================
// AUDIO TASK: WAKE WORD + ACTIVE MODE
// ============================================================

static bool assistant_active = false;
static int64_t last_user_activity_us = 0;
static int64_t connect_start_us = 0;

static void audio_task(void *arg)
{
    (void)arg;
    static uint8_t audio_buffer[4096];
    size_t buffer_pos = 0;
    uint32_t silent_frames = 0;
    int64_t last_silent_log_us = 0;

    // WakeNet diagnostics: idle-only, no change to the detection flow.
    static int64_t last_debug_us = 0;
    static int detect_calls = 0;
    static int last_wake_result = 0;

    while (1) {
        // audio_read_mic() returns PCM16 mono at 16 kHz.
        size_t bytes_read = audio_read_mic(audio_buffer + buffer_pos,
                                           sizeof(audio_buffer) - buffer_pos);
        if (bytes_read > 0) buffer_pos += bytes_read;

        if (!assistant_active) {
            // ========== MODE IDLE: tunggu "Hi, ESP" ==========
            // Diagnostic setiap 1 detik. Tidak mengubah logika utama.
            int64_t now_debug_us = esp_timer_get_time();
            if (now_debug_us - last_debug_us >= 1000000) {
                last_debug_us = now_debug_us;
                int32_t max_abs = 0;
                size_t wake_bytes = (size_t)wake_chunk_samples * sizeof(int16_t);

                if (wake_chunk_samples > 0 && buffer_pos >= wake_bytes) {
                    int16_t *pcm = reinterpret_cast<int16_t *>(audio_buffer);
                    for (int i = 0; i < wake_chunk_samples; ++i) {
                        int32_t val = pcm[i];
                        int32_t magnitude = val < 0 ? -val : val;
                        if (magnitude > max_abs) max_abs = magnitude;
                    }
                }

                ESP_LOGI("WAKE_DEBUG",
                         "buffer_pos=%u max_abs=%ld detect_calls=%d last_result=%d chunk_samples=%d bytes_read=%u",
                         (unsigned)buffer_pos,
                         (long)max_abs,
                         detect_calls,
                         last_wake_result,
                         wake_chunk_samples,
                         (unsigned)bytes_read);
            }

            // Feed WakeNet only complete model chunks. If the audio HAL
            // returns smaller/larger reads, accumulate them until a full
            // chunk is available instead of silently dropping samples.
            while (wake_iface && wake_model && wake_chunk_samples > 0 &&
                   buffer_pos >= (size_t)wake_chunk_samples * sizeof(int16_t)) {
                size_t wake_bytes = (size_t)wake_chunk_samples * sizeof(int16_t);
                int16_t *wake_pcm = reinterpret_cast<int16_t *>(audio_buffer);
                detect_calls++;
                int wake_result = wake_iface->detect(wake_model, wake_pcm);
                last_wake_result = wake_result;

                if (wake_result > 0) {
                    ESP_LOGW(TAG, ">>> WAKE WORD TERDETEKSI: HI, ESP (id=%d)", wake_result);
                    assistant_active = true;
                    connect_start_us = esp_timer_get_time();
                    last_user_activity_us = connect_start_us;
                    websocket_app_start();
                    buffer_pos = 0;
                    vTaskDelay(pdMS_TO_TICKS(10));
                    continue;
                }

                size_t remainder = buffer_pos - wake_bytes;
                if (remainder > 0) memmove(audio_buffer, audio_buffer + wake_bytes, remainder);
                buffer_pos = remainder;
            }

            // Tombol BOOT tetap dipertahankan sebagai fallback manual.
            if (gpio_get_level(BOOT_BUTTON_GPIO) == 0) {
                vTaskDelay(pdMS_TO_TICKS(50));
                if (gpio_get_level(BOOT_BUTTON_GPIO) == 0) {
                    while (gpio_get_level(BOOT_BUTTON_GPIO) == 0) {
                        vTaskDelay(pdMS_TO_TICKS(10));
                    }
                    ESP_LOGI(TAG, "Tombol ditekan! Memulai sesi...");
                    assistant_active = true;
                    connect_start_us = esp_timer_get_time();
                    last_user_activity_us = connect_start_us;
                    websocket_app_start();
                }
            }

            vTaskDelay(pdMS_TO_TICKS(2));
            continue;
        }

        // ========== MODE AKTIF ==========
        if (!websocket_is_connected()) {
            if (esp_timer_get_time() - connect_start_us > 15 * 1000000LL) {
                ESP_LOGW(TAG, "Koneksi gagal. Kembali ke mode idle.");
                assistant_active = false;
                buffer_pos = 0;
                continue;
            }
            buffer_pos = 0;
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        if (buffer_pos >= 3200) {
            bool has_activity = mic_frame_has_activity(audio_buffer, 3200);

            if (has_activity) {
                last_user_activity_us = esp_timer_get_time();
            }

            int64_t now_us = esp_timer_get_time();
            if (now_us - last_user_activity_us > 60 * 1000000LL) {
                ESP_LOGI(TAG, "Idle 60 detik, menutup sesi.");
                assistant_active = false;
                websocket_disconnect();
                buffer_pos = 0;
                continue;
            }

            if (!audio_turn_active) {
                if (has_activity) {
                    websocket_send_audio_data(audio_buffer, 3200);
                } else {
                    silent_frames++;
                    int64_t now_log = esp_timer_get_time();
                    if (last_silent_log_us == 0 || now_log - last_silent_log_us >= 1000000) {
                        last_silent_log_us = now_log;
                        ESP_LOGI(TAG, "V7.0.36 MIC TX gate: silent frames dropped=%lu",
                                 (unsigned long)silent_frames);
                    }
                }
            }

            size_t remainder = buffer_pos - 3200;
            if (remainder > 0) memmove(audio_buffer, audio_buffer + 3200, remainder);
            buffer_pos = remainder;
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// ============================================================
// APP MAIN
// ============================================================

extern "C" void app_main()
{
    ESP_LOGI("MAIN", "Total PSRAM: %d bytes", esp_psram_get_size());
    ESP_LOGI("MAIN", "Free Heap: %d bytes", esp_get_free_heap_size());
    ESP_LOGI("MAIN", "Free PSRAM: %d bytes", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

    ESP_LOGI(TAG, "ESP32-S3 Asisten Kamar Dimulai...");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    oled_init();
    display_status("Booting...");

    audio_hal_init();
    audio_i2s_test_tone();

    // WakeNet9 Hi ESP menggunakan PCM16 mono 16 kHz dari INMP441.
    if (!wakeword_init()) {
        ESP_LOGE(TAG, "WakeNet init gagal. Sistem tetap bisa dimulai dengan tombol BOOT.");
        display_status("WakeNet gagal!");
    } else {
        display_status("Katakan: Hi, ESP");
    }

    gpio_set_direction(BOOT_BUTTON_GPIO, GPIO_MODE_INPUT);
    gpio_set_pull_mode(BOOT_BUTTON_GPIO, GPIO_PULLUP_ONLY);
    ESP_LOGI(TAG, "Tombol boot siap di GPIO0");

    display_status("Menghubungkan WiFi...");
    wifi_init_sta();

    if (!wifi_wait_for_connection(15000)) {
        ESP_LOGE(TAG, "Wi-Fi tidak mendapatkan IP.");
        display_status("WiFi Gagal!");
        while (1) vTaskDelay(pdMS_TO_TICKS(1000));
    }

    esp_wifi_set_ps(WIFI_PS_NONE);
    ESP_LOGI(TAG, "WiFi power save dimatikan");

    ESP_LOGI(TAG, "WIFI READY - lanjut ke NTP");
    sync_sntp_time();
    ESP_LOGI(TAG, "Menunggu 1 detik...");
    vTaskDelay(pdMS_TO_TICKS(1000));

    debug_network_path();
    vTaskDelay(pdMS_TO_TICKS(1000));

    display_status("Sistem siap. Katakan Hi, ESP...");

    BaseType_t task_result = xTaskCreate(audio_task, "audio_task", 10240, NULL, 5, NULL);
    if (task_result != pdPASS)
        ESP_LOGE(TAG, "Gagal membuat audio_task!");
    else
        ESP_LOGI(TAG, "audio_task berhasil dimulai.");

    while (1) vTaskDelay(pdMS_TO_TICKS(1000));
}
