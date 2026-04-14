#pragma once

#include <Arduino.h>
#include <vector>
#include "display_manager.h"

struct AlarmEntry {
  int hour;
  int minute;
  bool enabled;
  bool firedToday;
};

struct ReminderEntry {
  String text;
  unsigned long dueMs;
  bool done;
};

class TaskManager {
public:
  TaskManager();

  String processCommand(const String& transcript);

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
  String reminderTask(const String& text);
  String questionsTask(const String& text);

  bool matchWeather(const String& text) const;
  bool matchAlarm(const String& text) const;
  bool matchTimer(const String& text) const;
  bool matchReminder(const String& text) const;
  bool matchQuestion(const String& text) const;

  int parseDuration(const String& text) const;
  bool parseAlarmTime(const String& text, int& hour, int& minute) const;
  unsigned long parseReminderDue(const String& text) const;

  // state
  bool timerActive;
  int timerRemainingSec;
  unsigned long timerEndMs;

  std::vector<AlarmEntry> alarms;
  unsigned long lastAlarmRingMs;

  std::vector<ReminderEntry> reminders;
};
