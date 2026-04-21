#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include "config.h"
#include "wifi_manager.h"
#include "button_manager.h"
#include "audio_processor.h"
#include "display_manager.h"
#include "wav_server.h"
#include "tasks.h"
#include "led_manager.h"

enum class AppState { Idle, Recording, Ready, Error };
static AppState appState = AppState::Idle;

static unsigned long recordingStartMs = 0;
static TaskManager taskManager;

bool isRecordingCallback() {
  return appState == AppState::Recording;
}

String getMetaJson() {
  String json = "{";
  json += "\"timerActive\":" + String(taskManager.isTimerActive() ? "true" : "false");
  json += ",\"timerRemaining\":" + String(taskManager.getTimerRemaining());
  json += "}";
  return json;
}

void timerBuzz(int pulses = 3, int onMs = 200, int offMs = 100) {
  for (int i = 0; i < pulses; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(onMs);
    digitalWrite(BUZZER_PIN, LOW);
    delay(offMs);
  }
}

void displayClockNow() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    // if NTP not ready, show placeholder.
    displayOledShowTimer(0); // or something
    return;
  }
  displayOledShowTimer(timeinfo.tm_hour * 3600 + timeinfo.tm_min * 60 + timeinfo.tm_sec);
}

void startRecording() {
  if (appState == AppState::Recording) return;
  appState = AppState::Recording;
  recordingStartMs = millis();
  audioStartBuffering();
  displaySetStatus(StatusState::Recording, "Recording... hold button");
  Serial.println("--- Recording started ---");
}

void stopRecording() {
  if (appState != AppState::Recording) return;
  audioStopBuffering();
  int sampleCount = audioGetBufferSampleCount();
  int16_t* buffer = audioGetBuffer();
  wavServerSetAudioBuffer(buffer, sampleCount);
  appState = AppState::Ready;

  float recordedSeconds = (millis() - recordingStartMs) / 1000.0f;
  Serial.printf("--- Recording stopped: %d samples (%.2f s) ---\n", sampleCount, recordedSeconds);
  displaySetStatus(StatusState::Idle, "Ready");
  wavServerAutoTranscribe();
}

void ensureNetworkServer() {
  if (wifiEnsureConnected()) {
    if (!wavServerIsRunning()) {
      Serial.println("Wi-Fi reconnected, starting WAV server");
      wavServerInit();
    }
  } else {
    if (wavServerIsRunning()) {
      Serial.println("Wi-Fi lost, stopping WAV server");
      wavServerStop();
    }
  }
}

void connectWiFi() {
  wifiSetup();
  if (WiFi.status() == WL_CONNECTED) {
    wavServerInit();
  }
}

void setup() {
  Serial.begin(115200);
  delay(100);

  displayInit();
  ledInit();
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  displayBootAnimation();
  displaySetStatus(StatusState::Idle, "Booting...");

  buttonSetup();

  if (!audioInit()) {
    displaySetStatus(StatusState::Error, "Audio init failed");
    while (1) {
      delay(1000);
    }
  }

  connectWiFi();
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  struct tm timeinfo;
  if (getLocalTime(&timeinfo, 15000)) {
    Serial.printf("NTP time sync: %04d-%02d-%02d %02d:%02d:%02d\n", timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  } else {
    Serial.println("NTP time sync failed");
  }

  wavServerSetCallbacks(startRecording, stopRecording, isRecordingCallback, getMetaJson, [](const String& text) {
    return taskManager.processCommand(text);
  }, [](int secs) {
    taskManager.startTimer(secs);
  });

  displaySetStatus(StatusState::Idle, "Ready");
}

void loop() {
  wavServerHandleClients();
  ledUpdate();

  buttonUpdate();

  if (buttonWasPressed()) {
    startRecording();
  }

  if (buttonWasReleased()) {
    stopRecording();
  }

  if (appState == AppState::Recording) {
    float avgAbs;
    int level;
    int bars;
    if (audioReadAndCompute(avgAbs, level, bars)) {
      if (buttonIsDown()) {
        displayVisualizer(level, bars);
      } else {
        displayRecordingLevel(level, bars);
      }
      Serial.printf("REC: avg=%.0f lvl=%d bars=%d\n", avgAbs, level, bars);
    }
    // Keep sample reads continuous during recording.
    delay(1);
    return;
  }

  if (appState != AppState::Recording) {
    taskManager.loop();

    static unsigned long lastClockMs = 0;
    unsigned long nowMs = millis();
    if (!taskManager.isTimerActive() && nowMs - lastClockMs >= 1000) {
      lastClockMs = nowMs;
      displayClockNow();
    }
  }

  delay(50);
}
