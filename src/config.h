#pragma once

#include <Arduino.h>

// Wi-Fi and n8n
constexpr const char* WIFI_SSID = "harsha";
constexpr const char* WIFI_PASS = "12345678";

// Whisper server endpoint (configure to a reachable URL on your network)
// Example: "http://192.168.1.100:8080/transcribe"
constexpr const char* WHISPER_SERVER_URL = "http://10.252.126.134:8080/transcribe";

// I2S mic (INMP441 typical pins for ESP32-S3)
constexpr int I2S_PORT = 0;
constexpr int I2S_WS_PIN = 5;
constexpr int I2S_SCK_PIN = 4;
constexpr int I2S_SD_PIN = 6;

// I2S output (MAX98357A, separate I2S port to avoid sharing one-way mic path)
constexpr int I2S_OUT_PORT = 1;
constexpr int I2S_OUT_WS_PIN = 17;
constexpr int I2S_OUT_SCK_PIN = 16;
constexpr int I2S_OUT_SD_PIN = 18;
constexpr int I2S_OUT_SAMPLE_BITS = 16;

// INMP441 capture on this hardware is actually running at 8000 Hz (matches measured wall clock).
// Set to 8000 so WAV playback speed is real-time.
constexpr int SAMPLE_RATE = 8000;
constexpr int I2S_SAMPLE_BITS = 32;

// Button (do not use pin 0 due to boot/download conflict)
constexpr int BUTTON_PIN = 15;

// OLED (0.96 inch SSD1306 I2C default pins)
constexpr int OLED_SDA = 8;
constexpr int OLED_SCL = 9;
constexpr int OLED_ADDR = 0x3C;

// 4-digit 7-seg backpack (HT16K33) I2C address
constexpr int SEVEN_SEG_I2C_ADDR = 0x70;

// Buzzer pin
constexpr int BUZZER_PIN = 18;

// Logging and timings
constexpr int STATUS_UPDATE_MS = 100;
constexpr int RECORDING_DELAY_MS = 200;
constexpr int BUTTON_DEBOUNCE_MS = 20;

// Audio buffer (max 30s @ 16kHz 16-bit mono = ~960KB)
constexpr int MAX_AUDIO_SAMPLES = SAMPLE_RATE * 30;

// Mic audio sensitivity calibration
// Increase this value to reduce sensitivity / ignore more background noise.
constexpr int MIC_NOISE_FLOOR = 3000;

constexpr int HTTP_SERVER_PORT = 8080;

