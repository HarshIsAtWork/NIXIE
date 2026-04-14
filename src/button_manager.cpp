#include "button_manager.h"
#include "config.h"
#include <Arduino.h>

static int rawButtonState = HIGH;
static int debouncedButtonState = HIGH;
static unsigned long lastDebounce = 0;
static bool pressedEvent = false;
static bool releasedEvent = false;

void buttonSetup() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  rawButtonState = digitalRead(BUTTON_PIN);
  debouncedButtonState = rawButtonState;
  lastDebounce = millis();
  pressedEvent = false;
  releasedEvent = false;
}

void buttonUpdate() {
  int reading = digitalRead(BUTTON_PIN);
  if (reading != rawButtonState) {
    rawButtonState = reading;
    lastDebounce = millis();
  }

  if ((millis() - lastDebounce) > BUTTON_DEBOUNCE_MS) {
    if (debouncedButtonState != rawButtonState) {
      if (rawButtonState == LOW) {
        pressedEvent = true;
      } else {
        releasedEvent = true;
      }
      debouncedButtonState = rawButtonState;
    }
  }
}

bool buttonWasPressed() {
  if (pressedEvent) {
    pressedEvent = false;
    return true;
  }
  return false;
}

bool buttonWasReleased() {
  if (releasedEvent) {
    releasedEvent = false;
    return true;
  }
  return false;
}

bool buttonIsDown() {
  return debouncedButtonState == LOW;
}
