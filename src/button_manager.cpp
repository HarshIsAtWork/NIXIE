#include "button_manager.h"
#include "config.h"
#include <Arduino.h>

// Primary button state (legacy support)
static int rawButtonState = HIGH;
static int debouncedButtonState = HIGH;
static unsigned long lastDebounce = 0;
static bool pressedEvent = false;
static bool releasedEvent = false;

// Joystick state (analog + digital)
static int joystickVrxRaw = 2048;  // Center value for X-axis (0-4095)
static int joystickVryRaw = 2048;  // Center value for Y-axis (0-4095)
static int joystickSwRaw = HIGH;   // Button state (active LOW)
static int joystickSwDebounced = HIGH;

static bool joystickLeftEvent = false;
static bool joystickRightEvent = false;
static bool joystickUpEvent = false;
static bool joystickDownEvent = false;
static bool joystickButtonPressedEvent = false;
static bool joystickButtonReleasedEvent = false;

static unsigned long lastJoystickButtonDebounce = 0;
static unsigned long lastJoystickMoveMs = 0;
static int lastJoystickDirection = 0;  // 0=none, 1=left, 2=right, 3=up, 4=down
static constexpr unsigned long JOYSTICK_REPEAT_MS = 180;

void buttonSetup() {
  // Legacy button pin
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  rawButtonState = digitalRead(BUTTON_PIN);
  debouncedButtonState = rawButtonState;
  
  // Joystick pins
  pinMode(JOYSTICK_VRX_PIN, INPUT);
  pinMode(JOYSTICK_VRY_PIN, INPUT);
  pinMode(JOYSTICK_SW_PIN, INPUT_PULLUP);
  
  // ADC setup for ESP32 (12-bit, 0-4095 range)
  analogSetAttenuation(ADC_11db);  // Full range: 0-3.3V -> 0-4095
  
  lastDebounce = millis();
  lastJoystickButtonDebounce = millis();
  lastJoystickMoveMs = millis();
  pressedEvent = false;
  releasedEvent = false;
}

void buttonUpdate() {
  // Legacy button update
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

  // Joystick analog input update (X-axis and Y-axis)
  joystickVrxRaw = analogRead(JOYSTICK_VRX_PIN);  // 0-4095, center ~2048
  joystickVryRaw = analogRead(JOYSTICK_VRY_PIN);  // 0-4095, center ~2048

  // Joystick button (SW pin, active LOW)
  int swReading = digitalRead(JOYSTICK_SW_PIN);
  if (swReading != joystickSwRaw) {
    joystickSwRaw = swReading;
    lastJoystickButtonDebounce = millis();
  }
  if ((millis() - lastJoystickButtonDebounce) > JOYSTICK_DEBOUNCE_MS) {
    if (joystickSwDebounced != joystickSwRaw) {
      if (joystickSwRaw == LOW) {
        joystickButtonPressedEvent = true;
      } else {
        joystickButtonReleasedEvent = true;
      }
      joystickSwDebounced = joystickSwRaw;
    }
  }

  // Detect joystick direction based on ADC thresholds
  // VRx: < 1500 = left, > 2500 = right
  // VRy: < 1500 = up, > 2500 = down
  int currentDirection = 0;
  
  if (joystickVrxRaw < JOYSTICK_THRESHOLD) {
    currentDirection = 1;  // Left
  } else if (joystickVrxRaw > (4095 - JOYSTICK_THRESHOLD)) {
    currentDirection = 2;  // Right
  } else if (joystickVryRaw < JOYSTICK_THRESHOLD) {
    currentDirection = 3;  // Up
  } else if (joystickVryRaw > (4095 - JOYSTICK_THRESHOLD)) {
    currentDirection = 4;  // Down
  }

  if (currentDirection == 0) {
    lastJoystickDirection = 0;
    return;
  }

  unsigned long now = millis();
  if (currentDirection != lastJoystickDirection || (now - lastJoystickMoveMs) >= JOYSTICK_REPEAT_MS) {
    if (currentDirection == 1) joystickLeftEvent = true;
    else if (currentDirection == 2) joystickRightEvent = true;
    else if (currentDirection == 3) joystickUpEvent = true;
    else if (currentDirection == 4) joystickDownEvent = true;

    lastJoystickDirection = currentDirection;
    lastJoystickMoveMs = now;
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

// Joystick getter functions
bool joystickWasLeft() {
  if (joystickLeftEvent) {
    joystickLeftEvent = false;
    return true;
  }
  return false;
}

bool joystickWasRight() {
  if (joystickRightEvent) {
    joystickRightEvent = false;
    return true;
  }
  return false;
}

bool joystickWasUp() {
  if (joystickUpEvent) {
    joystickUpEvent = false;
    return true;
  }
  return false;
}

bool joystickWasDown() {
  if (joystickDownEvent) {
    joystickDownEvent = false;
    return true;
  }
  return false;
}

bool joystickButtonWasPressed() {
  if (joystickButtonPressedEvent) {
    joystickButtonPressedEvent = false;
    return true;
  }
  return false;
}

bool joystickButtonWasReleased() {
  if (joystickButtonReleasedEvent) {
    joystickButtonReleasedEvent = false;
    return true;
  }
  return false;
}

bool joystickButtonIsDown() {
  return joystickSwDebounced == LOW;
}

int getJoystickVrx() {
  return joystickVrxRaw;
}

int getJoystickVry() {
  return joystickVryRaw;
}
