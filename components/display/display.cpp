#include "display.h"
#include "esp_log.h"

static const char *TAG = "DISPLAY";

// OLED SEMENTARA DINONAKTIFKAN.
// Tujuan: menguji apakah I2C/OLED ikut menyebabkan audio speaker sendat.
// Fungsi tetap dipertahankan agar caller lain tidak perlu diubah.
// Tidak ada I2C bus yang dibuat dan tidak ada transaksi OLED.

void oled_init(void)
{
    ESP_LOGI(TAG, "OLED DISABLED - audio stability test");
}

void display_status(const char *text)
{
    // Tetap log status supaya alur program mudah dibaca,
    // tetapi tidak melakukan akses I2C/OLED.
    ESP_LOGI(TAG, "[OLED DISABLED]: %s", text ? text : "(null)");
}
