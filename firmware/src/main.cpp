#include <Arduino.h>
#include <driver/i2s.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include <WebServer.h>
#include "config.h"


// File handling
const char* RECORDING_FILE = "/recording.raw";
File audioFile;

// Recording state
bool isRecording = false;
int lastButtonState = HIGH;

// Add these definitions after your other constants
const char* ssid = WIFI_SSID;        // Change this to your WiFi name
const char* password = WIFI_PASSWORD; // Change this to your WiFi password
WebServer server(80);

// Add this with your other global variables
unsigned long recordingStartTime = 0;
size_t totalBytesWritten = 0;


void startRecording() {
    // Open file for writing
    audioFile = SPIFFS.open(RECORDING_FILE, FILE_WRITE);
    if (!audioFile) {
        Serial.println("Failed to open file for writing");
        return;
    }
    isRecording = true;
    recordingStartTime = millis();
    totalBytesWritten = 0;
    Serial.println("Started recording...");
}

void stopRecording() {

    unsigned long recordingDuration = (millis() - recordingStartTime) / 1000;

    audioFile.close();
    isRecording = false;
    
    Serial.printf("Recording stopped. Duration: %lu seconds, Bytes written: %u\n", 
        recordingDuration, totalBytesWritten);
}

void recordAudio() {
    int32_t buffer[I2S_BUFFER_SIZE];
    size_t bytesRead = 0;
    
    // Read audio data from I2S
    esp_err_t result = i2s_read(I2S_NUM_0, &buffer, sizeof(buffer), &bytesRead, portMAX_DELAY);
    
    if (result == ESP_OK && bytesRead > 0) {
        size_t bytesWritten = audioFile.write((const uint8_t*)buffer, bytesRead);
        totalBytesWritten += bytesWritten;
        
        static unsigned long lastPrint = 0;
        if (millis() - lastPrint > 1000) {
            Serial.printf("Bytes Written: %u, File Size: %u\n", 
                totalBytesWritten, audioFile.size());
            lastPrint = millis();
        }
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
    server.on("/recording", HTTP_GET, []() {
        File file = SPIFFS.open(RECORDING_FILE, FILE_READ);
        if (!file) {
            server.send(404, "text/plain", "File not found");
            return;
        }
        
        server.sendHeader("Content-Type", "audio/raw");
        server.sendHeader("Content-Disposition", "attachment; filename=recording.raw");
        server.streamFile(file, "audio/raw");
        file.close();
    });

    server.begin();
    Serial.println("Web server started");

    Serial.println("Setup complete - ready to record");
}

void loop() {

    static unsigned long lastPrint = 0;
    if (millis() - lastPrint > 5000) {  // Print every 5 seconds
        Serial.print("Server IP Address: ");
        Serial.println(WiFi.localIP());
        lastPrint = millis();
    }

    server.handleClient();

    bool currentButtonState = digitalRead(BUTTON_PIN);
    Serial.print("Current button state: ");
    Serial.println(currentButtonState ? "HIGH" : "LOW");

    Serial.print("Last button state: ");
    Serial.println(lastButtonState ? "HIGH" : "LOW");

    if (currentButtonState != lastButtonState) {
        Serial.println("Button state changed. Resetting debounce timer.");

                // Detect button press (HIGH to LOW transition)
        if (currentButtonState == LOW && lastButtonState == HIGH) {
            if (!isRecording) {
                Serial.println("Button pressed - starting recording.");
                startRecording();
            } else {
                Serial.println("Button pressed but already recording.");
            }
        }

        // Detect button release (LOW to HIGH transition)
        if (currentButtonState == HIGH && lastButtonState == LOW) {
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
    delay(100);

}
// void loop() {
//     // Read the button state
//     bool currentButtonState = digitalRead(BUTTON_PIN);
//     Serial.print("Current button state: ");
//     Serial.println(currentButtonState ? "HIGH" : "LOW");

//     // Debounce logic
//     if (currentButtonState != lastButtonState) {
//         lastDebounceTime = millis();
//         Serial.println("Button state changed. Resetting debounce timer.");
//     }

//     // Check if the debounce delay has passed
//     if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY) {
//         Serial.print("Debounce stable state detected: ");
//         Serial.println(currentButtonState ? "HIGH" : "LOW");

//         // Detect button press (HIGH to LOW transition)
//         if (currentButtonState == LOW && lastButtonState == HIGH) {
//             if (!isRecording) {
//                 Serial.println("Button pressed - starting recording.");
//                 startRecording();
//             } else {
//                 Serial.println("Button pressed but already recording.");
//             }
//         }

//         // Detect button release (LOW to HIGH transition)
//         if (currentButtonState == HIGH && lastButtonState == LOW) {
//             if (isRecording) {
//                 Serial.println("Button released - stopping recording.");
//                 stopRecording();
//             } else {
//                 Serial.println("Button released but not recording.");
//             }
//         }
//     } else {
//         Serial.println("Debounce delay active, ignoring button changes.");
//     }

//     // Update the last button state
//     lastButtonState = currentButtonState;

//     // Log recording activity
//     if (isRecording) {
//         Serial.println("Recording in progress...");
//         recordAudio();
//         digitalWrite(LIGHT_PIN, HIGH);
//     } else {
//         Serial.println("Not recording.");
//         digitalWrite(LIGHT_PIN, LOW);
//     }

//     delay(100);  // Small delay to avoid flooding the serial monitor
// }
