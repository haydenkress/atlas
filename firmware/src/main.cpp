#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <driver/i2s.h>

// WiFi credentials from .env
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;
const char* serverUrl = "http://" SERVER_IP ":3000/upload";

// Pin Definitions
const int BUTTON_PIN = D0;    // Button input

// I2S Pins for ICS-43434 Microphone
const int I2S_MIC_BCLK = D2;  // Bit clock
const int I2S_MIC_LRCL = D3;  // Word select / Left-right clock
const int I2S_MIC_DIN = D4;   // Data in from mic

// I2S Pins for MAX98357A Speaker
const int I2S_SPK_BCLK = D5;  // Bit clock
const int I2S_SPK_LRC = D6;   // Word select / Left-right clock
const int I2S_SPK_DIN = D7;   // Data out to speaker

const int BUFFER_SIZE = 1024;

// I2S configuration for microphone (I2S_NUM_0)
i2s_config_t i2s_mic_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = 16000,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_24BIT,  // ICS-43434 is 24-bit
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = BUFFER_SIZE,
    .use_apll = false,
    .tx_desc_auto_clear = true,
    .fixed_mclk = 0
};

// I2S configuration for speaker (I2S_NUM_1)
i2s_config_t i2s_spk_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = 44100,                    // MAX98357A supports up to 44.1kHz
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = BUFFER_SIZE,
    .use_apll = false,
    .tx_desc_auto_clear = true,
    .fixed_mclk = 0
};

// I2S pins for microphone
i2s_pin_config_t i2s_mic_pins = {
    .bck_io_num = I2S_MIC_BCLK,
    .ws_io_num = I2S_MIC_LRCL,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_MIC_DIN
};

// I2S pins for speaker
i2s_pin_config_t i2s_spk_pins = {
    .bck_io_num = I2S_SPK_BCLK,
    .ws_io_num = I2S_SPK_LRC,
    .data_out_num = I2S_SPK_DIN,
    .data_in_num = I2S_PIN_NO_CHANGE
};

HTTPClient http;
int32_t audioBuffer[BUFFER_SIZE];  // Changed to 32-bit for 24-bit samples
bool isRecording = false;

// Function to play audio received from server
void playAudio(const uint8_t* audioData, size_t audioLength) {
    size_t bytesWritten = 0;
    i2s_write(I2S_NUM_1, audioData, audioLength, &bytesWritten, portMAX_DELAY);
}

void setup() {
    Serial.begin(115200);
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    
    // Connect to WiFi
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi connected!");
    
    // Initialize I2S for microphone
    esp_err_t err = i2s_driver_install(I2S_NUM_0, &i2s_mic_config, 0, NULL);
    if (err != ESP_OK) {
        Serial.println("Failed to install I2S driver for mic");
        return;
    }
    
    err = i2s_set_pin(I2S_NUM_0, &i2s_mic_pins);
    if (err != ESP_OK) {
        Serial.println("Failed to set I2S pins for mic");
        return;
    }

    // Initialize I2S for speaker
    err = i2s_driver_install(I2S_NUM_1, &i2s_spk_config, 0, NULL);
    if (err != ESP_OK) {
        Serial.println("Failed to install I2S driver for speaker");
        return;
    }
    
    err = i2s_set_pin(I2S_NUM_1, &i2s_spk_pins);
    if (err != ESP_OK) {
        Serial.println("Failed to set I2S pins for speaker");
        return;
    }
}

void loop() {
    if (digitalRead(BUTTON_PIN) == LOW) {
        if (!isRecording) {
            isRecording = true;
            http.begin(serverUrl);
            http.addHeader("Content-Type", "application/octet-stream");
            Serial.println("Started recording");
        }
        
        size_t bytes_read = 0;
        i2s_read(I2S_NUM_0, audioBuffer, sizeof(audioBuffer), &bytes_read, portMAX_DELAY);
        
        if (bytes_read > 0) {
            // Convert 24-bit samples to 16-bit for server
            int16_t* convertedBuffer = (int16_t*)malloc(bytes_read/2);
            for(int i = 0; i < bytes_read/4; i++) {
                convertedBuffer[i] = audioBuffer[i] >> 8; // Convert 24-bit to 16-bit
            }
            
            int httpCode = http.POST((uint8_t*)convertedBuffer, bytes_read/2);
            if (httpCode == HTTP_CODE_OK) {
                // Get response and play through speaker
                String response = http.getString();
                if(response.length() > 0) {
                    playAudio((uint8_t*)response.c_str(), response.length());
                }
            }
            
            free(convertedBuffer);
        }
    } else if (isRecording) {
        http.end();
        isRecording = false;
        Serial.println("Stopped recording");
    }
    
    delay(10);
}