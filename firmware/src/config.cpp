#include "config.h"
#include <Arduino.h>

// Microphone I2S config
i2s_config_t i2s_mic_Config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = I2S_SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
    .dma_buf_len = 256,
    .use_apll = true,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0,
};

// Speaker I2S config
i2s_config_t i2s_speaker_Config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = I2S_SAMPLE_RATE,
    .bits_per_sample = i2s_bits_per_sample_t(I2S_BITS_PER_SAMPLE),
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
    .dma_buf_len = I2S_BUFFER_SIZE,
    .use_apll = false,
    .tx_desc_auto_clear = true,
    .fixed_mclk = 0
};

// Microphone pins config
i2s_pin_config_t i2s_mic_pins = {
    .mck_io_num = I2S_PIN_NO_CHANGE,
    .bck_io_num = I2S_MIC_SERIAL_CLOCK,
    .ws_io_num = I2S_MIC_LEFT_RIGHT_CLOCK,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_MIC_SERIAL_DATA
};

// Speaker pins config
i2s_pin_config_t i2s_speaker_pins = {
    .mck_io_num = I2S_PIN_NO_CHANGE,
    .bck_io_num = I2S_SPEAKER_SERIAL_CLOCK,
    .ws_io_num = I2S_SPEAKER_LEFT_RIGHT_CLOCK,
    .data_out_num = I2S_SPEAKER_SERIAL_DATA,
    .data_in_num = I2S_PIN_NO_CHANGE
};

void i2sInit() {
    // Initialize I2S0 for microphone
    esp_err_t err = i2s_driver_install(I2S_MIC_PORT, &i2s_mic_Config, 0, NULL);
    if (err != ESP_OK) {
        Serial.printf("Failed installing microphone driver: %d\n", err);
        return;
    }

    err = i2s_set_pin(I2S_MIC_PORT, &i2s_mic_pins);
    if (err != ESP_OK) {
        Serial.printf("Failed setting microphone pins: %d\n", err);
        return;
    }

    // Initialize I2S1 for speaker
    err = i2s_driver_install(I2S_SPEAKER_PORT, &i2s_speaker_Config, 0, NULL);
    if (err != ESP_OK) {
        Serial.printf("Failed installing speaker driver: %d\n", err);
        return;
    }

    err = i2s_set_pin(I2S_SPEAKER_PORT, &i2s_speaker_pins);
    if (err != ESP_OK) {
        Serial.printf("Failed setting speaker pins: %d\n", err);
        return;
    }

    Serial.println("I2S initialized successfully");
}