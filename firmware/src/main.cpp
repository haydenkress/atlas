#include <Arduino.h>
#include <driver/i2s.h>
#include <WiFi.h>
#include <WebServer.h>
#include "config.h"
#include "AudioTools.h"
#include <LittleFS.h>

// File handling
const char* RECORDING_FILE = "/recording.wav";
File audioFile;

// Recording state
bool isRecording = false;
int lastButtonState = LOW; // see if this works

// Add these definitions after your other constants
const char* ssid = WIFI_SSID;        // Change this to your WiFi name
const char* password = WIFI_PASSWORD; // Change this to your WiFi password
WebServer server(80);

// Add this with your other global variables
unsigned long recordingStartTime = 0;
size_t totalBytesWritten = 0;

void writeWavHeader(File &file, uint32_t dataSize) {
    // WAV header for PCM format
    uint8_t header[44] = {
        'R', 'I', 'F', 'F',
        0, 0, 0, 0, // ChunkSize (to be updated)
        'W', 'A', 'V', 'E',
        'f', 'm', 't', ' ',
        16, 0, 0, 0, // Subchunk1Size (16 for PCM)
        1, 0, // AudioFormat (1 for PCM)
        1, 0, // NumChannels (1 for mono)
        0x40, 0x3E, 0, 0, // SampleRate
        0x80, 0x7D, 0, 0, // ByteRate
        2, 0, // BlockAlign (NumChannels * BitsPerSample/8)
        16, 0, // BitsPerSample
        'd', 'a', 't', 'a',
        0, 0, 0, 0 // Subchunk2Size (to be updated)
    };

    uint32_t chunkSize = 36 + dataSize;
    uint32_t subchunk2Size = dataSize;

    // Update header with actual values
    memcpy(&header[4], &chunkSize, 4);
    memcpy(&header[40], &subchunk2Size, 4);

    file.seek(0);
    file.write(header, 44);
}


void startRecording() {
    // Open file for writing
    audioFile = LittleFS.open(RECORDING_FILE, FILE_WRITE);
    if (!audioFile) {
        Serial.println("Failed to open file for writing");
        return;
    }

    isRecording = true;
    recordingStartTime = millis();
    totalBytesWritten = 0;
    Serial.println("Started recording...");
}

void recordAudio() {
    int32_t i2sReadBuffer[I2S_BUFFER_SIZE / 4];
    size_t bytesRead = 0;
    static unsigned long sampleCount = 0;
    static unsigned long startTime = millis();
    
    // Read audio data from I2S
    esp_err_t result = i2s_read(I2S_MIC_PORT, (char*)i2sReadBuffer, I2S_BUFFER_SIZE, &bytesRead, portMAX_DELAY);
    if (result != ESP_OK || bytesRead == 0) {
        return;
    }

     size_t samplesRead = bytesRead / sizeof(int32_t);
        sampleCount += samplesRead; 

    static int16_t wavBuffer[I2S_BUFFER_SIZE / 4];

    for (size_t i = 0; i < samplesRead; i++) {
    // Assuming left-justified data, take the top 16 bits
    int32_t sample32 = i2sReadBuffer[i];
    int16_t sample16 = (int16_t)(sample32 >> 16); // Cast to 16-bit signed
    wavBuffer[i] = sample16;
    }

    size_t bytesToWrite = samplesRead * sizeof(int16_t);
    size_t bytesWritten = audioFile.write((uint8_t*)wavBuffer, bytesToWrite);

    totalBytesWritten += bytesWritten;

    static unsigned long lastPrint = 0;
    if (millis() - lastPrint > 1000) {
        float elapsedSeconds = (millis() - recordingStartTime) / 1000.0;
        float actualSampleRate = sampleCount / elapsedSeconds;
        Serial.printf("Bytes Written: %u, File Size: %u\n", totalBytesWritten, audioFile.size());
        Serial.printf("Actual sample rate: %.2f Hz\n", actualSampleRate);
        Serial.printf("Buffer Read: %d bytes, Written: %d bytes\n", bytesRead, bytesWritten);
        lastPrint = millis();
    }
}

void stopRecording() {
    unsigned long recordingDuration = (millis() - recordingStartTime) / 1000;

    // write WAV header with actual sizes
    writeWavHeader(audioFile, totalBytesWritten);

    audioFile.close();
    isRecording = false;

    unsigned long expectedBytes = recordingDuration * I2S_SAMPLE_RATE * I2S_BITS_PER_SAMPLE / 8;
    
    Serial.printf("Recording stopped. Duration: %lu seconds, Bytes written: %u\n", 
        recordingDuration, totalBytesWritten);
    Serial.printf("Expected bytes: %lu\n", expectedBytes);
}


void setup() {
    Serial.begin(115200);
    while(!Serial) delay(100);  // Wait for serial to be ready

    Serial.println("\n\nInitializing Audio Recorder...");

    // Initialize button/light pins
    pinMode(BUTTON_PIN, INPUT);
    pinMode(LIGHT_PIN, OUTPUT);

    // Initialize LittleFS
    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS initialization failed!");
        return;
    }

    // Initialize I2S
    i2sInit();

    // Add WiFi and server setup
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Connecting to WiFi...");
    }
    Serial.println("Connected to WiFi");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());

    // Setup web server endpoint
    server.on("/recording.wav", HTTP_GET, []() {
        File file = LittleFS.open(RECORDING_FILE, FILE_READ);
        if (!file) {
            server.send(404, "text/plain", "File not found");
            return;
        }

        server.streamFile(file, "audio/wav");
        file.close();
    });

    server.begin();
    Serial.println("Web server started");

    Serial.println("Setup complete - ready to record");
}

void loop() {
    server.handleClient();

    bool currentButtonState = digitalRead(BUTTON_PIN);

    if (currentButtonState != lastButtonState) {
        // Detect button press (HIGH to LOW transition)
        if (currentButtonState == HIGH && lastButtonState == LOW) {
            if (!isRecording) {
                Serial.println("Button pressed - starting recording.");
                startRecording();
            } else {
                Serial.println("Button pressed but already recording.");
            }
        }

        // Detect button release (LOW to HIGH transition)
        if (currentButtonState == LOW && lastButtonState == HIGH) {
            if (isRecording) {
                Serial.println("Button released - stopping recording.");
                stopRecording();
            } else {
                Serial.println("Button released but not recording.");
            }
        }
    }

    if (isRecording) {
        recordAudio();
        digitalWrite(LIGHT_PIN, HIGH);
    } else {
        digitalWrite(LIGHT_PIN, LOW);
    }
    
    lastButtonState = currentButtonState;
    delay(10);
}