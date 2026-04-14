#include "wifi_manager.h"
#include "config.h"
#include <WiFi.h>

static unsigned long lastWifiAttemptMs = 0;

void wifiSetup() {
  Serial.printf("Connecting to Wi-Fi %s...\n", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    Serial.print(".");
    delay(300);
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\nWi-Fi connected: %s IP: %s\n", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
  } else {
    Serial.println("\nWi-Fi connection failed.");
  }

  lastWifiAttemptMs = millis();
}

bool wifiEnsureConnected() {
  if (WiFi.status() == WL_CONNECTED) {
    return true;
  }

  unsigned long now = millis();
  if (now - lastWifiAttemptMs < 10000) {
    return false;
  }

  Serial.println("Wi-Fi disconnected, retrying...");
  WiFi.disconnect(true);
  wifiSetup();
  return WiFi.status() == WL_CONNECTED;
}

bool wifiIsConnected() {
  return WiFi.status() == WL_CONNECTED;
}
