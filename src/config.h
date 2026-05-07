#pragma once

#include <Arduino.h>

// Wi-Fi and n8n
constexpr const char* WIFI_SSID = "harsha";
constexpr const char* WIFI_PASS = "12345678";

// Whisper server endpoint on the development PC.
// Use the LAN IP so the ESP32 can reach it reliably over Wi-Fi.
constexpr const char* WHISPER_SERVER_URL = "http://10.56.205.134:8080/transcribe";

// I2S mic (INMP441 typical pins for ESP32-S3)
constexpr int I2S_PORT = 0;
constexpr int I2S_WS_PIN = 5;
constexpr int I2S_SCK_PIN = 4;
constexpr int I2S_SD_PIN = 6;

constexpr int SAMPLE_RATE = 16000;
constexpr int I2S_SAMPLE_BITS = 32;

// Button (do not use pin 0 due to boot/download conflict)
constexpr int BUTTON_PIN = 17; // physical pushbutton for recording

// Rotary encoder for menu navigation. Wire CLK/DT/SW to these pins and GND.
// Note: GPIO2 is a boot strapping pin, so we use GPIO19 for the button instead


// Analog joystick pins (if present)
constexpr int JOYSTICK_VRX_PIN = 1;
constexpr int JOYSTICK_VRY_PIN = 2;
constexpr int JOYSTICK_SW_PIN = 19;
constexpr int JOYSTICK_DEBOUNCE_MS = 50;
constexpr int JOYSTICK_THRESHOLD = 900;
constexpr int JOYSTICK_CENTER = 512;

// 2.4 inch SPI TFT, usually ILI9341, wired in landscape mode.
// Touch pins are intentionally unused because this module's touch is unreliable.
constexpr int TFT_SCK_PIN = 12;
constexpr int TFT_MOSI_PIN = 11;
constexpr int TFT_MISO_PIN = 13;
constexpr int TFT_CS_PIN = 10;
constexpr int TFT_DC_PIN = 14;
constexpr int TFT_RESET_PIN = 21;
constexpr int TFT_LED_PIN = 7;

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
