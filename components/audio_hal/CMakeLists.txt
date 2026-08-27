idf_component_register(
    SRCS "audio_hal.cpp"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_driver_gpio esp_driver_i2s
)
