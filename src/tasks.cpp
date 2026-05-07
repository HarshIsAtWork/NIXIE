#include "tasks.h"
#include "config.h"
#include <time.h>
#include <math.h>
#include <string.h>

TaskManager::TaskManager()
    : timerActive(false), timerRemainingSec(0), timerEndMs(0), lastAlarmRingMs(0) {}

String TaskManager::processCommand(const String& transcript) {
  return tryProcessOfflineCommand(transcript);
}

String TaskManager::tryProcessOfflineCommand(const String& transcript) {
  if (matchMath(transcript)) {
    return mathTask(transcript);
  }
  if (matchPomodoro(transcript)) {
    return pomodoroTask(transcript);
  }
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

  return String();
}

static String trimCopy(const String& value) {
  String out = value;
  out.trim();
  return out;
}

static String formatHourMinute12(int hour, int minute) {
  int shownHour = hour % 12;
  if (shownHour == 0) shownHour = 12;
  char buf[24];
  snprintf(buf, sizeof(buf), "%d:%02d%s", shownHour, minute, hour >= 12 ? "PM" : "AM");
  return String(buf);
}

static String formatEpoch12(time_t epoch) {
  struct tm when;
  localtime_r(&epoch, &when);
  return formatHourMinute12(when.tm_hour, when.tm_min);
}

static bool nowLocal(struct tm& timeinfo, time_t& epoch) {
  time(&epoch);
  if (epoch <= 0) {
    return false;
  }
  localtime_r(&epoch, &timeinfo);
  return true;
}

static std::vector<long> extractIntegers(const String& text) {
  std::vector<long> values;
  String current;
  for (size_t i = 0; i < text.length(); i++) {
    char c = text[i];
    if (isDigit(c) || (c == '-' && current.length() == 0)) {
      current += c;
    } else if (current.length() > 0) {
      values.push_back(current.toInt());
      current = "";
    }
  }
  if (current.length() > 0) {
    values.push_back(current.toInt());
  }
  return values;
}

static long gcdLong(long a, long b) {
  a = labs(a);
  b = labs(b);
  while (b != 0) {
    long t = a % b;
    a = b;
    b = t;
  }
  return a;
}

static long lcmLong(long a, long b) {
  if (a == 0 || b == 0) return 0;
  return labs(a / gcdLong(a, b) * b);
}

static String factorizeNumber(long value) {
  if (value == 0) return "0";
  if (value == 1) return "1";

  String result;
  long n = labs(value);
  if (value < 0) {
    result = "-1";
  }

  for (long p = 2; p * p <= n; p++) {
    int count = 0;
    while (n % p == 0) {
      n /= p;
      count++;
    }
    if (count == 0) continue;
    if (result.length() > 0) result += " x ";
    result += String(p);
    if (count > 1) {
      result += "^";
      result += String(count);
    }
  }

  if (n > 1) {
    if (result.length() > 0) result += " x ";
    result += String(n);
  }

  return result;
}

class ExpressionParser {
public:
  ExpressionParser(const String& input, float xValue = 0.0f, bool allowX = false)
      : text(input), x(xValue), useX(allowX), pos(0), ok(true) {
    text.replace(" ", "");
    text.toLowerCase();
    text.replace("y=", "");
  }

  bool parse(float& out) {
    out = parseExpression();
    skipSpaces();
    return ok && pos >= text.length() && isfinite(out);
  }

private:
  String text;
  float x;
  bool useX;
  int pos;
  bool ok;

  void skipSpaces() {
    while (pos < text.length() && text[pos] == ' ') pos++;
  }

  bool consume(char c) {
    skipSpaces();
    if (pos < text.length() && text[pos] == c) {
      pos++;
      return true;
    }
    return false;
  }

  float parseExpression() {
    float v = parseTerm();
    while (ok) {
      if (consume('+')) {
        v += parseTerm();
      } else if (consume('-')) {
        v -= parseTerm();
      } else {
        break;
      }
    }
    return v;
  }

  float parseTerm() {
    float v = parsePower();
    while (ok) {
      if (consume('*')) {
        v *= parsePower();
      } else if (consume('/')) {
        float d = parsePower();
        if (fabs(d) < 0.00001f) {
          ok = false;
          return 0;
        }
        v /= d;
      } else {
        break;
      }
    }
    return v;
  }

  float parsePower() {
    float v = parseUnary();
    if (consume('^')) {
      v = pow(v, parsePower());
    }
    return v;
  }

  float parseUnary() {
    if (consume('+')) return parseUnary();
    if (consume('-')) return -parseUnary();
    return parsePrimary();
  }

  bool consumeName(const char* name) {
    int start = pos;
    for (int i = 0; name[i]; i++) {
      if (start + i >= text.length() || text[start + i] != name[i]) return false;
    }
    pos += strlen(name);
    return true;
  }

  float parseFunction(const char* name) {
    if (!consume('(')) {
      ok = false;
      return 0;
    }
    float v = parseExpression();
    if (!consume(')')) ok = false;
    if (strcmp(name, "sin") == 0) return sin(v);
    if (strcmp(name, "cos") == 0) return cos(v);
    if (strcmp(name, "tan") == 0) return tan(v);
    if (strcmp(name, "asin") == 0) return asin(v);
    if (strcmp(name, "acos") == 0) return acos(v);
    if (strcmp(name, "atan") == 0) return atan(v);
    if (strcmp(name, "sqrt") == 0) return sqrt(max(0.0f, v));
    if (strcmp(name, "abs") == 0) return fabs(v);
    return v;
  }

  float parsePrimary() {
    skipSpaces();
    if (consume('(')) {
      float v = parseExpression();
      if (!consume(')')) ok = false;
      return v;
    }

    if (consumeName("asin")) return parseFunction("asin");
    if (consumeName("acos")) return parseFunction("acos");
    if (consumeName("atan")) return parseFunction("atan");
    if (consumeName("sin")) return parseFunction("sin");
    if (consumeName("cos")) return parseFunction("cos");
    if (consumeName("tan")) return parseFunction("tan");
    if (consumeName("sqrt")) return parseFunction("sqrt");
    if (consumeName("abs")) return parseFunction("abs");
    if (consumeName("pi")) return PI;

    if (pos < text.length() && text[pos] == 'x') {
      pos++;
      if (!useX) ok = false;
      return x;
    }

    int start = pos;
    bool dotSeen = false;
    while (pos < text.length()) {
      char c = text[pos];
      if (isDigit(c)) {
        pos++;
      } else if (c == '.' && !dotSeen) {
        dotSeen = true;
        pos++;
      } else {
        break;
      }
    }
    if (start == pos) {
      ok = false;
      return 0;
    }
    return text.substring(start, pos).toFloat();
  }
};

static String extractMathExpression(const String& text, bool& graphMode) {
  String expr = text;
  expr.toLowerCase();
  graphMode = expr.indexOf("graph") >= 0 || expr.indexOf("plot") >= 0 || expr.indexOf("y=") >= 0;

  expr.replace("show me the graph of", "");
  expr.replace("show the graph of", "");
  expr.replace("graph of", "");
  expr.replace("plot of", "");
  expr.replace("plot", "");
  expr.replace("graph", "");
  expr.replace("show me", "");
  expr.replace("show", "");

  const char* prefixes[] = {"calculate", "compute", "solve", "what is", "what's"};
  for (const char* p : prefixes) {
    int idx = expr.indexOf(p);
    if (idx >= 0) {
      expr = expr.substring(idx + strlen(p));
      break;
    }
  }
  expr.replace("equals", "");
  expr.replace("equal to", "");
  expr.replace("plus", "+");
  expr.replace("minus", "-");
  expr.replace("times", "*");
  expr.replace("multiplied by", "*");
  expr.replace("divided by", "/");
  expr.replace("over", "/");
  expr.replace("pi", "pi");
  expr.replace("tan inverse", "atan");
  expr.replace("arctan", "atan");
  expr.replace("sin inverse", "asin");
  expr.replace("arcsin", "asin");
  expr.replace("cos inverse", "acos");
  expr.replace("arccos", "acos");
  expr.replace("of ", "");
  expr.replace("atanx", "atan(x)");
  expr.replace("asinx", "asin(x)");
  expr.replace("acosx", "acos(x)");
  expr.trim();
  int yIdx = expr.indexOf("y=");
  if (yIdx >= 0) expr = expr.substring(yIdx + 2);
  expr.trim();
  return expr;
}

bool TaskManager::matchMath(const String& text) const {
  String lower = text;
  lower.toLowerCase();
  if (lower.indexOf("calculate") >= 0 || lower.indexOf("compute") >= 0 || lower.indexOf("solve") >= 0 || lower.indexOf("graph") >= 0 || lower.indexOf("plot") >= 0) return true;
  if (lower.indexOf("lcm") >= 0 || lower.indexOf("hcf") >= 0 || lower.indexOf("gcd") >= 0 || lower.indexOf("factor") >= 0) return true;
  if (lower.indexOf("tan inverse") >= 0 || lower.indexOf("sin inverse") >= 0 || lower.indexOf("cos inverse") >= 0) return true;
  return lower.indexOf('+') >= 0 || lower.indexOf('*') >= 0 || lower.indexOf('/') >= 0 || lower.indexOf('^') >= 0 || lower.indexOf("y=") >= 0;
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

bool TaskManager::matchPomodoro(const String& text) const {
  String lower = text;
  lower.toLowerCase();
  return lower.indexOf("pomodoro") >= 0 || lower.indexOf("focus session") >= 0 || lower.indexOf("focus timer") >= 0;
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

String TaskManager::mathTask(const String& text) {
  String lower = text;
  lower.toLowerCase();
  std::vector<long> values = extractIntegers(lower);

  if (lower.indexOf("lcm") >= 0 && values.size() >= 2) {
    long answer = labs(values[0]);
    for (size_t i = 1; i < values.size(); i++) {
      answer = lcmLong(answer, values[i]);
    }
    String response = "LCM is " + String(answer);
    displayShowOutput(response);
    displayShowMathTool("lcm", String(answer), false);
    return response;
  }

  if ((lower.indexOf("hcf") >= 0 || lower.indexOf("gcd") >= 0) && values.size() >= 2) {
    long answer = labs(values[0]);
    for (size_t i = 1; i < values.size(); i++) {
      answer = gcdLong(answer, values[i]);
    }
    String response = "HCF is " + String(answer);
    displayShowOutput(response);
    displayShowMathTool("hcf", String(answer), false);
    return response;
  }

  if (lower.indexOf("factor") >= 0 && !values.empty()) {
    String factors = factorizeNumber(values[0]);
    String response = "Factorization of " + String(values[0]) + " is " + factors;
    displayShowOutput(response);
    displayShowMathTool(String(values[0]), factors, false);
    return response;
  }

  bool graphMode = false;
  String expr = extractMathExpression(text, graphMode);
  if (expr.length() == 0) {
    return "Math tool needs an expression, for example calculate 12*(3+4) or graph y=x^2.";
  }

  float value = 0.0f;
  ExpressionParser parser(expr, 0.0f, graphMode);
  bool ok = graphMode ? true : parser.parse(value);

  String result;
  if (graphMode) {
    result = String("y = ") + expr;
  } else if (ok) {
    char buf[48];
    snprintf(buf, sizeof(buf), "= %.4g", value);
    result = String(buf);
  } else {
    result = "Could not parse expression";
  }

  String response = graphMode ? String("Showing graph of y = ") + expr : String("Math tool: ") + expr + " " + result;
  displayShowOutput(response);
  displayShowMathTool(expr, result, graphMode);
  return response;
}

String TaskManager::weatherTask(const String& text) {
  String response = "Weather: 24 C clear. (Device offline placeholder)";
  displaySetWeatherSummary("24 C, clear", "Offline placeholder");
  displaySetStatus(StatusState::Idle, response.c_str());
  displayShowOutput(response);
  return response;
}

bool TaskManager::parseAlarmTime(const String& text, int& outHour, int& outMin) const {
  String lower = text;
  lower.toLowerCase();
  lower.replace(".", "");

  for (int i = 0; i < lower.length(); i++) {
    if (!isDigit(lower[i])) continue;

    int j = i;
    String hourStr;
    while (j < lower.length() && isDigit(lower[j])) {
      hourStr += lower[j++];
    }

    int minute = 0;
    if (j < lower.length() && lower[j] == ':') {
      j++;
      String minuteStr;
      while (j < lower.length() && isDigit(lower[j])) {
        minuteStr += lower[j++];
      }
      if (minuteStr.length() == 0) continue;
      minute = minuteStr.toInt();
    }

    while (j < lower.length() && lower[j] == ' ') j++;

    bool hasAm = lower.substring(j).startsWith("am");
    bool hasPm = lower.substring(j).startsWith("pm");
    int hour = hourStr.toInt();

    if (minute < 0 || minute > 59) continue;
    if (hasAm || hasPm) {
      if (hour < 1 || hour > 12) continue;
      if (hasPm && hour < 12) hour += 12;
      if (hasAm && hour == 12) hour = 0;
    } else if (hour > 23) {
      continue;
    }

    outHour = hour;
    outMin = minute;
    return true;
  }

  return false;
}

String TaskManager::alarmTask(const String& text) {
  int hour, minute;
  if (!parseAlarmTime(text, hour, minute)) {
    return "Alarm not set, could not parse time. Use 'alarm at 7:30' or 'set alarm at 7 am'.";
  }

  struct tm nowInfo;
  time_t nowEpoch;
  if (!nowLocal(nowInfo, nowEpoch)) {
    return "Alarm not set because local time is unavailable.";
  }

  struct tm alarmInfo = nowInfo;
  alarmInfo.tm_hour = hour;
  alarmInfo.tm_min = minute;
  alarmInfo.tm_sec = 0;

  bool tomorrow = text.indexOf("tomorrow") >= 0;
  time_t alarmEpoch = mktime(&alarmInfo);
  if (tomorrow || alarmEpoch <= nowEpoch) {
    alarmInfo.tm_mday += 1;
    alarmEpoch = mktime(&alarmInfo);
  }

  alarms.push_back({alarmEpoch, formatEpoch12(alarmEpoch), true, false});
  String response = "Alarm set " + formatEpoch12(alarmEpoch);
  refreshPlanSummaryDisplay();
  displaySetStatus(StatusState::Idle, response.c_str());
  displayShowOutput(response);
  return response;
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
    displayOpenTimer();
    return "Timer window opened.";
  }
  startTimer(seconds);
  char buf[64];
  snprintf(buf, sizeof(buf), "Timer started for %d sec", seconds);
  return String(buf);
}

String TaskManager::pomodoroTask(const String& text) {
  String lower = text;
  lower.toLowerCase();
  bool startNow = lower.indexOf("start") >= 0 || lower.indexOf("begin") >= 0;
  displayOpenPomodoro(startNow);
  return startNow ? "Pomodoro started." : "Pomodoro window opened.";
}

String TaskManager::reminderTask(const String& text) {
  time_t dueEpoch = parseReminderDue(text);
  if (dueEpoch <= 0) {
    return "Reminder not set, could not parse when it is due.";
  }

  String lower = text;
  lower.toLowerCase();
  String reminderText = lower;
  int start = reminderText.indexOf("remind me to ");
  if (start >= 0) {
    reminderText = reminderText.substring(start + 13);
  }

  const char* markers[] = {" tomorrow", " today", " at ", " in "};
  for (const char* marker : markers) {
    int idx = reminderText.indexOf(marker);
    if (idx >= 0) {
      reminderText = reminderText.substring(0, idx);
      break;
    }
  }
  reminderText.trim();
  if (reminderText.length() == 0) {
    reminderText = "Reminder";
  }

  reminders.push_back({reminderText, dueEpoch, false});

  String response = "Reminder set " + reminderText + " " + formatEpoch12(dueEpoch);
  refreshPlanSummaryDisplay();
  displayShowOutput(response);
  displaySetStatus(StatusState::Idle, response.c_str());
  return response;
}

void TaskManager::getPlanSummaries(String& alarmLine, String& reminderLine, String& taskLine) const {
  time_t nowEpoch = time(nullptr);

  const AlarmEntry* nextAlarm = nullptr;
  for (const AlarmEntry& alarm : alarms) {
    if (!alarm.enabled || alarm.fired) continue;
    if (!nextAlarm || alarm.triggerEpoch < nextAlarm->triggerEpoch) {
      nextAlarm = &alarm;
    }
  }
  alarmLine = nextAlarm ? String("Next ") + formatEpoch12(nextAlarm->triggerEpoch) : "No alarms set";

  const ReminderEntry* nextReminder = nullptr;
  for (const ReminderEntry& reminder : reminders) {
    if (reminder.done) continue;
    if (!nextReminder || reminder.triggerEpoch < nextReminder->triggerEpoch) {
      nextReminder = &reminder;
    }
  }
  if (nextReminder) {
    String reminderText = nextReminder->text;
    if (reminderText.length() > 18) reminderText = reminderText.substring(0, 15) + "...";
    reminderLine = reminderText + " " + formatEpoch12(nextReminder->triggerEpoch);
  } else {
    reminderLine = "No reminders";
  }

  int alarmCount = 0;
  for (const AlarmEntry& alarm : alarms) {
    if (alarm.enabled && !alarm.fired && alarm.triggerEpoch >= nowEpoch) alarmCount++;
  }
  int reminderCount = 0;
  for (const ReminderEntry& reminder : reminders) {
    if (!reminder.done) reminderCount++;
  }
  if (alarmCount + reminderCount > 0) {
    String parts;
    if (alarmCount > 0) parts += String(alarmCount) + " alarm" + (alarmCount == 1 ? "" : "s");
    if (reminderCount > 0) {
      if (parts.length() > 0) parts += ", ";
      parts += String(reminderCount) + " reminder" + (reminderCount == 1 ? "" : "s");
    }
    taskLine = parts + " pending";
  } else {
    taskLine = "No tasks scheduled";
  }
}

void TaskManager::refreshPlanSummaryDisplay() {
  String alarmLine;
  String reminderLine;
  String taskLine;
  getPlanSummaries(alarmLine, reminderLine, taskLine);
  displaySetScheduleSummary(alarmLine, reminderLine, taskLine);
}

time_t TaskManager::parseReminderDue(const String& text) const {
  String lower = text;
  lower.toLowerCase();

  int idx = lower.indexOf("in ");
  struct tm nowInfo;
  time_t nowEpoch;
  if (!nowLocal(nowInfo, nowEpoch)) {
    return 0;
  }

  if (idx >= 0) {
    idx += 3;
    while (idx < lower.length() && !isDigit(lower[idx])) idx++;

    String num;
    while (idx < lower.length() && isDigit(lower[idx])) {
      num += lower[idx++];
    }

    int value = num.toInt();
    if (value <= 0) value = 1;

    if (lower.indexOf("hour", idx) >= 0 || lower.indexOf("hr", idx) >= 0) {
      return nowEpoch + (time_t)value * 3600;
    }
    return nowEpoch + (time_t)value * 60;
  }

  int hour = 0;
  int minute = 0;
  if (!parseAlarmTime(lower, hour, minute)) {
    return 0;
  }

  struct tm dueInfo = nowInfo;
  dueInfo.tm_hour = hour;
  dueInfo.tm_min = minute;
  dueInfo.tm_sec = 0;
  if (lower.indexOf("tomorrow") >= 0) {
    dueInfo.tm_mday += 1;
  }

  time_t dueEpoch = mktime(&dueInfo);
  if (lower.indexOf("today") < 0 && lower.indexOf("tomorrow") < 0 && dueEpoch <= nowEpoch) {
    dueInfo.tm_mday += 1;
    dueEpoch = mktime(&dueInfo);
  }

  return dueEpoch;
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
  display7SegClear();
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
  bool scheduleChanged = false;

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
    time_t nowEpoch = time(nullptr);
    for (auto& alarm : alarms) {
      if (!alarm.enabled || alarm.fired) continue;

      if (nowEpoch >= alarm.triggerEpoch) {
        unsigned long nowMs = millis();
        if (nowMs - lastAlarmRingMs > 30000UL) {
          String msg = "Alarm " + formatHourMinute12(now.tm_hour, now.tm_min);
          displaySetStatus(StatusState::Idle, "Alarm ringing");
          displayShowOutput(msg);
          for (int i = 0; i < 3; ++i) {
            digitalWrite(BUZZER_PIN, HIGH);
            delay(150);
            digitalWrite(BUZZER_PIN, LOW);
            delay(100);
          }
          alarm.fired = true;
          lastAlarmRingMs = nowMs;
          scheduleChanged = true;
        }
      }
    }
  }

  time_t nowEpoch = time(nullptr);
  for (auto& r : reminders) {
    if (r.done) continue;
    if (nowEpoch >= r.triggerEpoch) {
      r.done = true;
      String msg = "Reminder: " + r.text;
      displayShowOutput(msg);
      displaySetStatus(StatusState::Idle, msg.c_str());
      scheduleChanged = true;
    }
  }

  if (scheduleChanged) {
    refreshPlanSummaryDisplay();
  }
}
