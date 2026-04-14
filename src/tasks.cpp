#include "tasks.h"
#include "config.h"
#include <time.h>

TaskManager::TaskManager()
    : timerActive(false), timerRemainingSec(0), timerEndMs(0), lastAlarmRingMs(0) {}

String TaskManager::processCommand(const String& transcript) {
  if (matchTimer(transcript)) {
    return timerTask(transcript);
  }
  if (matchAlarm(transcript)) {
    return alarmTask(transcript);
  }
  if (matchReminder(transcript)) {
    return reminderTask(transcript);
  }
  if (matchWeather(transcript)) {
    return weatherTask(transcript);
  }
  if (matchQuestion(transcript)) {
    return questionsTask(transcript);
  }

  return "No task matched. Please try weather/alarm/timer/reminder/question.";
}

bool TaskManager::matchWeather(const String& text) const {
  String lower = text;
  lower.toLowerCase();
  return lower.indexOf("weather") >= 0 || lower.indexOf("forecast") >= 0;
}

bool TaskManager::matchAlarm(const String& text) const {
  String lower = text;
  lower.toLowerCase();
  return lower.indexOf("alarm") >= 0 || lower.indexOf("wake") >= 0;
}

bool TaskManager::matchTimer(const String& text) const {
  String lower = text;
  lower.toLowerCase();
  return lower.indexOf("timer") >= 0 || lower.indexOf("countdown") >= 0;
}

bool TaskManager::matchReminder(const String& text) const {
  String lower = text;
  lower.toLowerCase();
  return lower.indexOf("remind me") >= 0 || lower.indexOf("reminder") >= 0;
}

bool TaskManager::matchQuestion(const String& text) const {
  String lower = text;
  lower.toLowerCase();
  return lower.indexOf("what") >= 0 || lower.indexOf("who") >= 0 || lower.indexOf("when") >= 0|| lower.indexOf("how") >= 0 || lower.indexOf("why") >= 0 || lower.indexOf("?") >= 0;
}

String TaskManager::weatherTask(const String& text) {
  String response = "Weather: 24°C clear. (Device offline placeholder)";
  displaySetStatus(StatusState::Idle, response.c_str());
  displayShowOutput(response);
  return response;
}

bool TaskManager::parseAlarmTime(const String& text, int& outHour, int& outMin) const {
  String lower = text;
  lower.toLowerCase();

  int idx = lower.indexOf("at ");
  if (idx < 0) idx = lower.indexOf("for ");
  if (idx < 0) return false;
  idx += 3;

  String part = lower.substring(idx);
  part.trim();

  int colon = part.indexOf(':');
  if (colon > 0) {
    String h = part.substring(0, colon);
    String m;
    int i = colon + 1;
    while (i < part.length() && isDigit(part[i])) {
      m += part[i++];
    }
    int hour = h.toInt();
    int minute = m.toInt();
    if (hour >= 0 && hour < 24 && minute >= 0 && minute < 60) {
      outHour = hour;
      outMin = minute;
      return true;
    }
  }

  int space = part.indexOf(' ');
  String hstr = (space > 0 ? part.substring(0, space) : part);
  String suffix = (space > 0 ? part.substring(space + 1) : "");

  if (hstr.length() > 0) {
    int hour = hstr.toInt();
    if (hour > 0 && hour <= 12) {
      if (suffix.indexOf("pm") >= 0 && hour < 12) hour += 12;
      if (suffix.indexOf("am") >= 0 && hour == 12) hour = 0;
      outHour = hour;
      outMin = 0;
      return true;
    }
    if (hour >= 0 && hour <= 23) {
      outHour = hour;
      outMin = 0;
      return true;
    }
  }

  return false;
}

String TaskManager::alarmTask(const String& text) {
  int hour, minute;
  if (!parseAlarmTime(text, hour, minute)) {
    return "Alarm not set, could not parse time. Use 'alarm at 7:30' or 'set alarm at 7 am'.";
  }

  alarms.push_back({hour, minute, true, false});
  char buf[64];
  snprintf(buf, sizeof(buf), "Alarm set for %02d:%02d", hour, minute);
  displaySetStatus(StatusState::Idle, buf);
  displayShowOutput(buf);
  return String(buf);
}

int TaskManager::parseDuration(const String& text) const {
  String lower = text;
  lower.toLowerCase();
  int amount = 0;

  int idx = lower.indexOf("for ");
  if (idx >= 0) {
    idx += 4;
  } else {
    // fallback: find first digit in whole string
    idx = 0;
    while (idx < lower.length() && !isDigit(lower[idx])) {
      idx++;
    }
  }

  String num;
  while (idx < lower.length() && isDigit(lower[idx])) {
    num += lower[idx++];
  }

  if (num.length() > 0) {
    amount = num.toInt();
  }

  if (amount <= 0) {
    if (lower.indexOf("set timer") >= 0 || lower.indexOf("start timer") >= 0) {
      return 60;
    }
    return 0;
  }

  if (lower.indexOf("hour") >= 0 || lower.indexOf("hr") >= 0) {
    return amount * 3600;
  }
  if (lower.indexOf("min") >= 0) {
    return amount * 60;
  }
  if (lower.indexOf("sec") >= 0) {
    return amount;
  }

  return amount * 60;
}

String TaskManager::timerTask(const String& text) {
  int seconds = parseDuration(text);
  Serial.printf("timerTask received: '%s' -> seconds=%d\n", text.c_str(), seconds);
  if (seconds <= 0) {
    Serial.println("timerTask: invalid duration");
    return "Timer command is missing duration. Try 'set timer for 5 minutes'.";
  }
  startTimer(seconds);
  char buf[64];
  snprintf(buf, sizeof(buf), "Timer started for %d sec", seconds);
  displayShowOutput(buf);
  return String(buf);
}

String TaskManager::reminderTask(const String& text) {
  unsigned long dueMs = parseReminderDue(text);
  if (dueMs <= millis()) {
    dueMs = millis() + 30000;
  }

  String reminderText = text;
  int i = reminderText.indexOf("remind me to ");
  if (i >= 0) {
    reminderText = reminderText.substring(i + 12);
  }

  reminders.push_back({reminderText, dueMs, false});

  unsigned long deltaMinutes = (dueMs > millis()) ? (dueMs - millis()) / 60000 : 0;
  char buf[128];
  snprintf(buf, sizeof(buf), "Reminder saved: %s in %lumin", reminderText.c_str(), deltaMinutes);
  displayShowOutput(buf);
  displaySetStatus(StatusState::Idle, buf);
  return String(buf);
}

unsigned long TaskManager::parseReminderDue(const String& text) const {
  String lower = text;
  lower.toLowerCase();

  int idx = lower.indexOf("in ");
  if (idx < 0) return millis() + 60000;
  idx += 3;

  while (idx < lower.length() && !isDigit(lower[idx])) idx++;

  String num;
  while (idx < lower.length() && isDigit(lower[idx])) {
    num += lower[idx++];
  }

  int value = num.toInt();
  if (value <= 0) value = 1;

  if (lower.indexOf("hour", idx) >= 0 || lower.indexOf("hr", idx) >= 0) {
    return millis() + (unsigned long)value * 3600 * 1000UL;
  }
  return millis() + (unsigned long)value * 60 * 1000UL;
}

String TaskManager::questionsTask(const String& text) {
  String response = "Question noted. Answer unavailable offline, placeholder issued.";
  displayShowOutput(response);
  return response;
}

void TaskManager::startTimer(int seconds) {
  Serial.printf("startTimer: seconds=%d\n", seconds);
  if (seconds <= 0) {
    stopTimer();
    return;
  }
  timerActive = true;
  timerRemainingSec = seconds;
  timerEndMs = millis() + (unsigned long)seconds * 1000UL;
  displayOledShowTimer(timerRemainingSec);
  displaySetStatus(StatusState::Idle, "Timer started");
}

void TaskManager::stopTimer() {
  timerActive = false;
  timerRemainingSec = 0;
  display7SegClear();
  displaySetStatus(StatusState::Idle, "Timer stopped");
}

bool TaskManager::isTimerActive() const {
  return timerActive;
}

int TaskManager::getTimerRemaining() const {
  return timerRemainingSec;
}

void TaskManager::loop() {
  if (timerActive) {
    long remaining = (long)((timerEndMs - millis()) / 1000);
    if (remaining <= 0) {
      timerActive = false;
      timerRemainingSec = 0;
      displayOledShowTimer(0);
      displaySetStatus(StatusState::Idle, "Timer done");
      for (int i = 0; i < 4; ++i) {
        digitalWrite(BUZZER_PIN, HIGH);
        delay(100);
        digitalWrite(BUZZER_PIN, LOW);
        delay(90);
      }
    } else if (remaining != timerRemainingSec) {
      timerRemainingSec = remaining;
      displayOledShowTimer(timerRemainingSec);
      if ((timerRemainingSec % 60) == 0) {
        char buf[64];
        snprintf(buf, sizeof(buf), "Timer: %d min", timerRemainingSec / 60);
        displaySetStatus(StatusState::Idle, buf);
      }
    }
  }

  struct tm now;
  if (getLocalTime(&now)) {
    for (auto& alarm : alarms) {
      if (!alarm.enabled) continue;

      if (alarm.hour == now.tm_hour && alarm.minute == now.tm_min) {
        if (!alarm.firedToday) {
          unsigned long nowMs = millis();
          if (nowMs - lastAlarmRingMs > 30000UL) {
            displaySetStatus(StatusState::Idle, "Alarm ringing");
            displayScrollText("Alarm!", 160);
            for (int i = 0; i < 3; ++i) {
              digitalWrite(BUZZER_PIN, HIGH);
              delay(150);
              digitalWrite(BUZZER_PIN, LOW);
              delay(100);
            }
            alarm.firedToday = true;
            lastAlarmRingMs = nowMs;
          }
        }
      } else {
        alarm.firedToday = false;
      }
    }
  }

  unsigned long nowMs = millis();
  for (auto& r : reminders) {
    if (r.done) continue;
    if (r.dueMs <= nowMs) {
      r.done = true;
      String msg = "Reminder: " + r.text;
      displayShowOutput(msg);
      displayScrollText(msg, 200);
    }
  }
}
