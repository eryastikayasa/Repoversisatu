#include "websocket_internal.h"

#include "esp_log.h"

static const char *TAG = "WS_TX";

/*
 * Audio capture task TIDAK lagi melakukan TLS/WebSocket write.
 * Fungsi ini hanya menyalin frame PCM16 ke TX queue.
 *
 * Semua esp_websocket_client_send_text() dilakukan oleh
 * websocket_tx_task() di websocket_mgr.cpp.
 *
 * V7.0.14:
 * Queue menggunakan kebijakan realtime: jika penuh, frame lama
 * dapat diganti oleh frame terbaru di TX manager. Tujuannya
 * mencegah backlog audio ketika jaringan sedang tersendat.
 */
void websocket_send_audio_data(
    const uint8_t *data,
    size_t len
)
{
    if (!data || len == 0) {
        return;
    }

    if (len > WS_TX_AUDIO_SIZE) {
        ESP_LOGE(
            TAG,
            "Audio frame terlalu besar: %u byte",
            (unsigned)len
        );
        return;
    }

    if (!is_connected ||
        !setup_complete ||
        websocket_tx_error) {
        return;
    }

    uint32_t generation =
        websocket_connection_generation;

    if (!websocket_tx_enqueue_audio(
            data,
            len,
            generation)) {
        ESP_LOGD(
            TAG,
            "Audio frame tidak masuk TX queue"
        );
    }
}
