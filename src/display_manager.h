#pragma once

#include <Arduino.h>

enum class StatusState { Idle, WaitingForButton, Recording, Sending, Error };

void displayInit();
void displayShowWifiConnecting(const char* ssid);
void displayShowWifiResult(bool connected, const String& detail);
void displayShowDashboard();
void displayNavigateMenu(int delta);
void displaySelectMenu();
void displayReleaseMenu();
void displayUiTick();
void displayOpenTimer();
void displayOpenPomodoro(bool startNow);
void displaySetServiceStatus(bool webpageOnline, bool whisperOnline);
void displaySetWeatherSummary(const String& line1, const String& line2);
void displaySetScheduleSummary(const String& alarmLine, const String& reminderLine, const String& eventLine);
void displayShowMathTool(const String& expression, const String& result, bool showGraph);
void displaySetStatus(StatusState state, const char* msg = nullptr);
void displayRecordingLevel(int level, int bars);
void displayVisualizer(int level, int bars);
void displayBootAnimation();
void displayProcessingStep(int step);
void displayTypewriter(const String& text);
void displayShowOutput(const String& text);
void displayScrollText(const String& text, int scrollSpeedMs = 250);
void displayShowTranscription(const String& text);
void displayJoystickLeftRight(int delta);  // For month navigation in calendar

void display7SegInit();
void display7SegClear();
void displayOledShowTimer(int totalSeconds);
void display7SegShowClock(int hours, int minutes);
