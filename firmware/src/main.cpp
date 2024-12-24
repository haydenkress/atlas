#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <driver/i2s.h>
#include "config.h"  // Contains WIFI_SSID, WIFI_PASSWORD, SERVER_IP

// --- WiFi Credentials ---
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

// --- Server URL ---
String serverUrl = "http://" + String(SERVER_IP) + ":3000/upload";

// --- Button & LED Pins ---
#define LIGHT_VIN 1
#define BTN_PIN 2

// --- I2S for Microphone (ICS-43434) ---
#define I2S_MIC_BCLK 5
#define I2S_MIC_RLCL 3
#define I2S_MIC_DOUT 4

// --- I2S for Speaker (MAX98357A) ---
#define I2S_AMP_BCLK 10
#define I2S_AMP_LRCLK 9
#define I2S_AMP_DIN 11

// Buffer sizes
static const size_t RECORD_BUFFER_SIZE = 16000 * 2; // e.g. 1 second at 16kHz * 2 bytes
int16_t recordBuffer[RECORD_BUFFER_SIZE]; 
size_t currentPos = 0;

// I2S config for microphone
i2s_config_t i2s_mic_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = 16000,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 1024,
    .use_apll = false,
    .tx_desc_auto_clear = true,
    .fixed_mclk = 0
};

i2s_pin_config_t i2s_mic_pins = {
    .bck_io_num = I2S_MIC_BCLK,
    .ws_io_num = I2S_MIC_RLCL,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_MIC_DOUT
};

// I2S config for speaker
i2s_config_t i2s_spk_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = 44100,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 1024,
    .use_apll = false,
    .tx_desc_auto_clear = true,
    .fixed_mclk = 0
};

i2s_pin_config_t i2s_spk_pins = {
    .bck_io_num = I2S_AMP_BCLK,
    .ws_io_num = I2S_AMP_LRCLK,
    .data_out_num = I2S_AMP_DIN,
    .data_in_num = I2S_PIN_NO_CHANGE
};

bool isRecording = false;
HTTPClient http;

// ----------------------
// Play the TTS response
// ----------------------
void playServerAudio(WiFiClient &stream) {
  // Read data in chunks from the stream and write to I2S speaker
  uint8_t buffer[512];
  while (stream.connected() || stream.available()) {
    int sizeRead = stream.read(buffer, sizeof(buffer));
    if (sizeRead > 0) {
      size_t bytesWritten = 0;
      i2s_write(I2S_NUM_1, buffer, sizeRead, &bytesWritten, portMAX_DELAY);
    } else {
      // If no data, break or yield
      delay(1);
    }
  }

  Serial.println("Done playing audio from server!");
}

// ---------------
// Upload function
// ---------------
void uploadAudio(const uint8_t* audioData, size_t length) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("No WiFi, cannot upload");
    return;
  }

  http.begin(serverUrl);
  // Our server expects raw bytes: application/octet-stream
  http.addHeader("Content-Type", "application/octet-stream");

  // POST the entire audio
  int httpCode = http.POST((uint8_t*)audioData, length);
  if (httpCode > 0) {
    Serial.printf("HTTP POST code: %d\n", httpCode);
    if (httpCode == HTTP_CODE_OK) {
      // We expect audio bytes in the response
      WiFiClient* stream = http.getStreamPtr();
      playServerAudio(*stream);
    }
  } else {
    Serial.printf("HTTP POST failed, error: %s\n", http.errorToString(httpCode).c_str());
  }

  http.end();
}



void setup() {
  Serial.begin(115200);
  pinMode(BTN_PIN, INPUT_PULLUP);
  pinMode(LIGHT_VIN, OUTPUT);
  digitalWrite(LIGHT_VIN, LOW);

  // Connect WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");

  // I2S: Microphone
  esp_err_t err = i2s_driver_install(I2S_NUM_0, &i2s_mic_config, 0, NULL);
  if (err != ESP_OK) {
    Serial.println("Failed to install I2S driver for mic");
  }
  err = i2s_set_pin(I2S_NUM_0, &i2s_mic_pins);
  if (err != ESP_OK) {
    Serial.println("Failed to set I2S pins for mic");
  }

  // I2S: Speaker
  err = i2s_driver_install(I2S_NUM_1, &i2s_spk_config, 0, NULL);
  if (err != ESP_OK) {
    Serial.println("Failed to install I2S driver for speaker");
  }
  err = i2s_set_pin(I2S_NUM_1, &i2s_spk_pins);
  if (err != ESP_OK) {
    Serial.println("Failed to set I2S pins for speaker");
  }
}

void loop() {
  // Check button state
  if (digitalRead(BTN_PIN) == LOW) {
    // BUTTON PRESSED => Start or continue recording
    if (!isRecording) {
      Serial.println("Start recording");
      isRecording = true;
      currentPos = 0;
      
    }
    digitalWrite(LIGHT_VIN, HIGH);
    // Keep reading from I2S into recordBuffer
    size_t bytesRead = 0;
    i2s_read(I2S_NUM_0, &recordBuffer[currentPos], 1024, &bytesRead, 10);
    // bytesRead is in bytes; each sample is 2 bytes => # of samples = bytesRead/2
    currentPos += (bytesRead / 2);
    if (currentPos >= RECORD_BUFFER_SIZE) {
      // Buffer full (avoid overflow)
      Serial.println("Buffer full, stopping recording to avoid overflow");
      isRecording = false;
      digitalWrite(LIGHT_VIN, LOW);
    }
  }
  else {
    // BUTTON NOT PRESSED => if we were recording, now we stop and upload
    if (isRecording) {
      Serial.println("Stop recording, now upload to server...");
      isRecording = false;
      digitalWrite(LIGHT_VIN, LOW);

      // 1) We have currentPos * 2 bytes of valid audio in recordBuffer
      size_t totalBytes = currentPos * 2;
      uploadAudio((uint8_t*)recordBuffer, totalBytes);
    }
  }

  delay(10); // small delay
}