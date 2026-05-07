#pragma once

void buttonSetup();
void buttonUpdate();
bool buttonWasPressed();
bool buttonWasReleased();
bool buttonIsDown();

// Joystick direction events (analog input based)
bool joystickWasLeft();
bool joystickWasRight();
bool joystickWasUp();
bool joystickWasDown();

// Joystick button events (SW pin, digital)
bool joystickButtonWasPressed();
bool joystickButtonWasReleased();
bool joystickButtonIsDown();

// Raw ADC values (0-4095)
int getJoystickVrx();
int getJoystickVry();
