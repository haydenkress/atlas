#ifndef CONFIG_H
#define CONFIG_H

#include <freertos/FreeRTOS.h>
#include <driver/i2s.h>

// WiFi Settings
#define WIFI_SSID "MiNE"
#define WIFI_PASSWORD "hayden456"
#define SERVER_IP "69.108.44.80"

// Pin Definitions
#define BUTTON_PIN 1
#define LIGHT_PIN 2

// I2S Settings
#define I2S_MIC_PORT I2S_NUM_0
#define I2S_SPEAKER_PORT I2S_NUM_1
#define I2S_SAMPLE_RATE 16000
#define I2S_BUFFER_SIZE 1024
#define I2S_BITS_PER_SAMPLE 16

// I2S Mic Pins
#define I2S_MIC_SERIAL_CLOCK 7
#define I2S_MIC_LEFT_RIGHT_CLOCK 9
#define I2S_MIC_SERIAL_DATA 8

// I2S Speaker Pins
#define I2S_SPEAKER_SERIAL_CLOCK 5
#define I2S_SPEAKER_LEFT_RIGHT_CLOCK 6
#define I2S_SPEAKER_SERIAL_DATA 4

// I2S Configurations
extern i2s_config_t i2s_mic_Config;
extern i2s_config_t i2s_speaker_Config;
extern i2s_pin_config_t i2s_mic_pins;
extern i2s_pin_config_t i2s_speaker_pins;

// Function declarations
void i2sInit();

#endif