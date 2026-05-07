#pragma once

#include <Arduino.h>
#include <time.h>
#include <vector>
#include "display_manager.h"

struct AlarmEntry {
  time_t triggerEpoch;
  String label;
  bool enabled;
  bool fired;
};

struct ReminderEntry {
  String text;
  time_t triggerEpoch;
  bool done;
};

class TaskManager {
public:
  TaskManager();

  String processCommand(const String& transcript);
  String tryProcessOfflineCommand(const String& transcript);
  void getPlanSummaries(String& alarmLine, String& reminderLine, String& taskLine) const;

  void startTimer(int seconds);
  void stopTimer();
  bool isTimerActive() const;
  int getTimerRemaining() const;

  void loop();

private:
  // task implementations
  String weatherTask(const String& text);
  String alarmTask(const String& text);
  String timerTask(const String& text);
  String pomodoroTask(const String& text);
  String reminderTask(const String& text);
  String questionsTask(const String& text);
  String mathTask(const String& text);

  bool matchMath(const String& text) const;
  bool matchWeather(const String& text) const;
  bool matchAlarm(const String& text) const;
  bool matchPomodoro(const String& text) const;
  bool matchTimer(const String& text) const;
  bool matchReminder(const String& text) const;
  bool matchQuestion(const String& text) const;

  int parseDuration(const String& text) const;
  bool parseAlarmTime(const String& text, int& hour, int& minute) const;
  time_t parseReminderDue(const String& text) const;
  void refreshPlanSummaryDisplay();

  // state
  bool timerActive;
  int timerRemainingSec;
  unsigned long timerEndMs;

  std::vector<AlarmEntry> alarms;
  unsigned long lastAlarmRingMs;

  std::vector<ReminderEntry> reminders;
};
