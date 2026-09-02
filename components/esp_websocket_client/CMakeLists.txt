cmake_minimum_required(VERSION 3.16)

# Local build-time override for Espressif esp_websocket_client 1.8.0.
#
# The stock 1.8.0 client treats esp_transport_poll_write() == 0 as a
# fatal transport error even when errno == 0. Espressif issue #942 tracks
# this exact timeout behavior. We keep the upstream 1.8.0 source intact
# except for the error predicate below, generated into the build directory.

set(ESP_WS_VERSION "websocket-v1.8.0")
set(ESP_WS_BASE_URL "https://raw.githubusercontent.com/espressif/esp-protocols/${ESP_WS_VERSION}/components/esp_websocket_client")

set(ESP_WS_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/esp_websocket_client_patched")
set(ESP_WS_INCLUDE_DIR "${ESP_WS_BUILD_DIR}/include")
set(ESP_WS_SOURCE "${ESP_WS_BUILD_DIR}/esp_websocket_client.c")
set(ESP_WS_HEADER "${ESP_WS_INCLUDE_DIR}/esp_websocket_client.h")

file(MAKE_DIRECTORY "${ESP_WS_INCLUDE_DIR}")

file(DOWNLOAD
    "${ESP_WS_BASE_URL}/esp_websocket_client.c"
    "${ESP_WS_SOURCE}"
    TLS_VERIFY ON
    STATUS ESP_WS_SOURCE_STATUS
    SHOW_PROGRESS
)
list(GET ESP_WS_SOURCE_STATUS 0 ESP_WS_SOURCE_STATUS_CODE)
if(NOT ESP_WS_SOURCE_STATUS_CODE EQUAL 0)
    message(FATAL_ERROR "Failed to download esp_websocket_client.c from Espressif: ${ESP_WS_SOURCE_STATUS}")
endif()

file(DOWNLOAD
    "${ESP_WS_BASE_URL}/include/esp_websocket_client.h"
    "${ESP_WS_HEADER}"
    TLS_VERIFY ON
    STATUS ESP_WS_HEADER_STATUS
    SHOW_PROGRESS
)
list(GET ESP_WS_HEADER_STATUS 0 ESP_WS_HEADER_STATUS_CODE)
if(NOT ESP_WS_HEADER_STATUS_CODE EQUAL 0)
    message(FATAL_ERROR "Failed to download esp_websocket_client.h from Espressif: ${ESP_WS_HEADER_STATUS}")
endif()

file(READ "${ESP_WS_SOURCE}" ESP_WS_SOURCE_TEXT)

set(ESP_WS_STOCK_CONDITION "if (wlen < 0 || (wlen == 0 && need_write != 0)) {")
set(ESP_WS_PATCHED_CONDITION "if (wlen < 0 || (wlen == 0 && need_write != 0 && (timeout == portMAX_DELAY || errno))) {")

string(FIND "${ESP_WS_SOURCE_TEXT}" "${ESP_WS_STOCK_CONDITION}" ESP_WS_CONDITION_POS)
if(ESP_WS_CONDITION_POS EQUAL -1)
    message(FATAL_ERROR "esp_websocket_client 1.8.0 source does not contain the expected send-timeout condition; refusing to build an unverified patch.")
endif()

string(REPLACE
    "${ESP_WS_STOCK_CONDITION}"
    "${ESP_WS_PATCHED_CONDITION}"
    ESP_WS_SOURCE_TEXT
    "${ESP_WS_SOURCE_TEXT}"
)

file(WRITE "${ESP_WS_SOURCE}" "${ESP_WS_SOURCE_TEXT}")

idf_component_register(
    SRCS "${ESP_WS_SOURCE}"
    INCLUDE_DIRS "${ESP_WS_INCLUDE_DIR}"
    REQUIRES lwip esp-tls tcp_transport http_parser esp_event
    PRIV_REQUIRES esp_timer
)

message(STATUS "Using local patched esp_websocket_client ${ESP_WS_VERSION} (errno=0 write-timeout fix)")
