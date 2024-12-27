#include <Arduino.h>
#include <driver/i2s.h>
#include <SPIFFS.h>

// Pin definitions
const int BUTTON_PIN = 1;  // Choose appropriate digital pin
const int LIGHT_PIN = 2;
const int I2S_BCLK = 3;     // I2S Serial Clock
const int I2S_DOUT = 4;      // I2S Serial Data
const int I2S_LRCL = 5;      // I2S Word Select

// I2S configuration
const int SAMPLE_RATE = 16000;
const int BITS_PER_SAMPLE = 16;
const int BUFFER_SIZE = 1024;

// File handling
const char* RECORDING_FILE = "/recording.raw";
File audioFile;

// Recording state
bool isRecording = false;
//debouncing
bool lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;  // Adjust this value if needed

void i2sInit() {
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = (i2s_bits_per_sample_t)BITS_PER_SAMPLE,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = BUFFER_SIZE,
        .use_apll = false,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0
    };

    i2s_pin_config_t pin_config = {
        .mck_io_num = I2S_PIN_NO_CHANGE,
        .bck_io_num = I2S_BCLK,
        .ws_io_num = I2S_LRCL,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = I2S_DOUT
    };

    // Install and start I2S driver
    i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_NUM_0, &pin_config);
}

void startRecording() {
    // Open file for writing
    audioFile = SPIFFS.open(RECORDING_FILE, FILE_WRITE);
    if (!audioFile) {
        Serial.println("Failed to open file for writing");
        return;
    }
    isRecording = true;
    Serial.println("Started recording...");
}

void stopRecording() {
    audioFile.close();
    isRecording = false;
    Serial.println("Stopped recording");
}

void recordAudio() {
    int16_t buffer[BUFFER_SIZE];
    size_t bytesRead = 0;

    // Read audio data from I2S
    esp_err_t result = i2s_read(I2S_NUM_0, &buffer, sizeof(buffer), &bytesRead, portMAX_DELAY);

    if (result == ESP_OK && bytesRead > 0) {
        audioFile.write((uint8_t*)buffer, bytesRead);
    }
}

void setup() {
    Serial.begin(115200);
    while(!Serial) delay(100);  // Wait for serial to be ready

    Serial.println("\n\nInitializing Audio Recorder...");

    // Initialize button/light pins
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    pinMode(LIGHT_PIN, OUTPUT);

    // Initialize SPIFFS
    if (!SPIFFS.begin(true)) {
        Serial.println("SPIFFS initialization failed!");
        return;
    }

    // Initialize I2S
    i2sInit();

    Serial.println("Setup complete - ready to record");
}

void loop() {
    // Check button state
    if (digitalRead(BUTTON_PIN) == LOW) {  // Button pressed
        if (!isRecording) {
            startRecording();
        }
        recordAudio();
        digitalWrite(LIGHT_PIN, HIGH);
    } else {  // Button released
        if (isRecording) {
            stopRecording();
        }
        digitalWrite(LIGHT_PIN, LOW);
    }
}