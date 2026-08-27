#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// ============================================================
// GEMINI API KEY
// ============================================================
// API key disediakan saat build melalui GitHub Actions secret:
// GEMINI_API_KEY
//
// Jangan simpan API key asli di source repository.
// ============================================================
#include "gemini_api_key.h"

// ============================================================
// GEMINI LIVE API WEBSOCKET
// ============================================================

#define WEBSOCKET_SERVER_URL \
    "wss://generativelanguage.googleapis.com/ws/google.ai.generativelanguage.v1beta.GenerativeService.BidiGenerateContent?key=" \
    GEMINI_API_KEY

void websocket_app_start(void);

void websocket_send_audio_data(const uint8_t *data, size_t len);

bool websocket_is_connected(void);
