#pragma once

#include <Arduino.h>

enum class StatusState { Idle, WaitingForButton, Recording, Sending, Error };

void displayInit();
void displaySetStatus(StatusState state, const char* msg = nullptr);
void displayRecordingLevel(int level, int bars);
void displayVisualizer(int level, int bars);
void displayBootAnimation();
void displayProcessingStep(int step);
void displayTypewriter(const String& text);
void displayShowOutput(const String& text);
void displayScrollText(const String& text, int scrollSpeedMs = 250);
void displayShowTranscription(const String& text);

void display7SegInit();
void display7SegClear();
void displayOledShowTimer(int totalSeconds);
void display7SegShowClock(int hours, int minutes);
