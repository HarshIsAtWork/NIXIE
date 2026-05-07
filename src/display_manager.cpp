#include "display_manager.h"
#include "config.h"
#include <SPI.h>
#include <WiFi.h>
#include <time.h>
#include <math.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <Fonts/FreeSerifItalic24pt7b.h>

static Adafruit_ILI9341 tft(TFT_CS_PIN, TFT_DC_PIN, TFT_RESET_PIN);

enum class ScreenMode { Home, Timer, Pomodoro, Focus, Decision, Calendar, Weather, Schedule, Assistant };

static bool display_ok = false;
static bool screenDrawn = false;
static bool screenActive = false;
static ScreenMode currentScreen = ScreenMode::Home;
static int selectedMenu = 0;
static StatusState currentState = StatusState::Idle;
static String currentStatus = "Ready";
static String lastTranscription = "Press record and talk.";
static String lastOutput = "Answers appear here.";
static String scheduleLine1 = "No alarms set";
static String scheduleLine2 = "No reminders yet";
static String scheduleLine3 = "No events today";
static String weatherLine1 = "24 C, clear";
static String weatherLine2 = "Offline placeholder";
static String mathExpr;
static String mathResult;
static bool mathVisible = false;
static bool graphVisible = false;
static bool manualTimerRunning = false;
static int manualTimerSeconds = 5 * 60;
static int manualTimerTotal = 5 * 60;
static unsigned long manualTimerEndMs = 0;
static bool pomodoroRunning = false;
static bool pomodoroBreak = false;
static int pomodoroRemaining = 25 * 60;
static unsigned long pomodoroEndMs = 0;
static bool focusRunning = false;
static unsigned long focusStartMs = 0;
static int focusElapsed = 0;
static constexpr int MAX_FOCUS_HISTORY = 8;
static int focusHistory[MAX_FOCUS_HISTORY] = {0};
static int focusHistoryCount = 0;
static bool decisionYes = true;
static bool decisionSpinning = false;
static unsigned long decisionSpinUntilMs = 0;
static int decisionFrame = 0;
static int currentBars = 0;
static int timerRemaining = 0;
static int timerTotal = 0;
static int typewriterChars = 0;
static int transcriptionChars = 0;
static unsigned long lastAssistantAnimMs = 0;
static unsigned long lastTranscriptAnimMs = 0;
static int assistantAnimFrame = 0;
static bool webpageOnline = false;
static bool whisperOnline = false;

static ScreenMode prevScreen = ScreenMode::Assistant;
static StatusState prevState = StatusState::Error;
static String prevStatus;
static String prevClock;
static String prevHeader;
static String prevOutput;
static String prevTranscription;
static String prevWeather1;
static String prevWeather2;
static String prevSchedule1;
static String prevSchedule2;
static String prevSchedule3;
static String prevMathExpr;
static String prevMathResult;
static bool prevMathVisible = false;
static bool prevGraphVisible = false;
static bool prevManualTimerRunning = false;
static int prevManualTimerSeconds = -1;
static int prevManualTimerTotal = -1;
static int prevPomodoroRemaining = -1;
static bool prevPomodoroRunning = false;
static bool prevPomodoroBreak = false;
static int prevFocusElapsed = -1;
static bool prevFocusRunning = false;
static int prevFocusHistoryCount = -1;
static bool prevDecisionYes = false;
static bool prevDecisionSpinning = false;
static int prevDecisionFrame = -1;
static String prevCalendarDay;
static int prevMenu = -1;
static int prevBars = -1;
static int prevTimerRemaining = -1;
static int prevTimerTotal = -1;
static int prevTypewriterChars = -1;
static int prevTranscriptionChars = -1;

static constexpr int16_t W = 320;
static constexpr int16_t H = 240;
static constexpr int16_t HEADER_Y = 0;
static constexpr int16_t HEADER_H = 26;
static constexpr int16_t STATUS_Y = 29;
static constexpr int16_t STATUS_H = 17;
static constexpr int16_t CONTENT_Y = 50;
static constexpr int16_t CONTENT_H = 158;
static constexpr int16_t NAV_Y = 213;
static constexpr int16_t NAV_H = 27;
static constexpr uint16_t C_BG = 0x0841;
static constexpr uint16_t C_SURFACE = 0x18E3;
static constexpr uint16_t C_SURFACE_2 = 0x2126;
static constexpr uint16_t C_LINE = 0x5AEB;
static constexpr uint16_t C_TEXT = ILI9341_WHITE;
static constexpr uint16_t C_MUTED = 0xA514;
static constexpr uint16_t C_BLUE = 0x04BF;
static constexpr uint16_t C_GREEN = 0x35E6;
static constexpr uint16_t C_AMBER = 0xFD20;
static constexpr uint16_t C_ORANGE = 0xFB20;
static constexpr uint16_t C_RED = 0xE8E4;
static constexpr uint16_t C_CYAN = 0x37FF;

static constexpr int MENU_COUNT = 9;
static const char* menuNames[] = {"HOME", "TIM", "POMO", "FOC", "YES?", "CAL", "WX", "PLAN", "ASK"};

static String fitText(const String& text, int maxChars) {
  if (text.length() <= maxChars) return text;
  if (maxChars <= 3) return text.substring(0, maxChars);
  return text.substring(0, maxChars - 3) + "...";
}

static uint16_t blend565(uint16_t a, uint16_t b, uint8_t amount) {
  uint8_t ar = ((a >> 11) & 0x1F) << 3;
  uint8_t ag = ((a >> 5) & 0x3F) << 2;
  uint8_t ab = (a & 0x1F) << 3;
  uint8_t br = ((b >> 11) & 0x1F) << 3;
  uint8_t bg = ((b >> 5) & 0x3F) << 2;
  uint8_t bb = (b & 0x1F) << 3;
  uint8_t r = ar + (((int)br - ar) * amount) / 255;
  uint8_t g = ag + (((int)bg - ag) * amount) / 255;
  uint8_t blue = ab + (((int)bb - ab) * amount) / 255;
  return tft.color565(r, g, blue);
}

static void resetDirtyCache() {
  prevScreen = ScreenMode::Assistant;
  prevState = StatusState::Error;
  prevStatus = "";
  prevClock = "";
  prevHeader = "";
  prevOutput = "";
  prevTranscription = "";
  prevWeather1 = "";
  prevWeather2 = "";
  prevSchedule1 = "";
  prevSchedule2 = "";
  prevSchedule3 = "";
  prevMathExpr = "";
  prevMathResult = "";
  prevMathVisible = !mathVisible;
  prevGraphVisible = !graphVisible;
  prevManualTimerRunning = !manualTimerRunning;
  prevManualTimerSeconds = -1;
  prevManualTimerTotal = -1;
  prevPomodoroRemaining = -1;
  prevPomodoroRunning = !pomodoroRunning;
  prevPomodoroBreak = !pomodoroBreak;
  prevFocusElapsed = -1;
  prevFocusRunning = !focusRunning;
  prevFocusHistoryCount = -1;
  prevDecisionYes = !decisionYes;
  prevDecisionSpinning = !decisionSpinning;
  prevDecisionFrame = -1;
  prevCalendarDay = "";
  prevMenu = -1;
  prevBars = -1;
  prevTimerRemaining = -1;
  prevTimerTotal = -1;
  prevTypewriterChars = -1;
  prevTranscriptionChars = -1;
}

static void drawText(int16_t x, int16_t y, const String& text, uint16_t color, uint8_t size = 1) {
  tft.setFont();
  tft.setTextSize(size);
  tft.setTextColor(color);
  tft.setCursor(x, y);
  tft.print(text);
}

static uint16_t statusColor(StatusState state) {
  switch (state) {
    case StatusState::Recording: return C_RED;
    case StatusState::Sending: return C_AMBER;
    case StatusState::Error: return C_RED;
    case StatusState::WaitingForButton: return C_BLUE;
    case StatusState::Idle:
    default: return C_GREEN;
  }
}

static const char* statusWord(StatusState state) {
  switch (state) {
    case StatusState::Recording: return "LISTEN";
    case StatusState::Sending: return "THINK";
    case StatusState::Error: return "CHECK";
    case StatusState::WaitingForButton: return "READY";
    case StatusState::Idle:
    default: return "READY";
  }
}

static String clockNow() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 10)) return "--:--";
  char buf[16];
  snprintf(buf, sizeof(buf), "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  return String(buf);
}

static int wrapText(const String& text, String* lines, int maxLines, int maxChars) {
  int count = 0;
  int idx = 0;
  while (idx < text.length() && count < maxLines) {
    while (idx < text.length() && text[idx] == ' ') idx++;
    int start = idx;
    int lastSpace = -1;
    int chars = 0;
    while (idx < text.length() && chars < maxChars && text[idx] != '\n') {
      if (text[idx] == ' ') lastSpace = idx;
      idx++;
      chars++;
    }
    int end = idx;
    if (idx < text.length() && text[idx] != '\n' && chars >= maxChars && lastSpace > start) {
      end = lastSpace;
      idx = lastSpace + 1;
    } else if (idx < text.length() && text[idx] == '\n') {
      idx++;
    }
    if (end <= start) {
      end = min(start + maxChars, int(text.length()));
      idx = end;
    }
    lines[count] = text.substring(start, end);
    lines[count].trim();
    count++;
  }
  return count;
}

static void renderWrappedLines(int16_t x, int16_t y, int16_t w, int16_t h, const String& text, int charsToShow, int startLine = -1, uint8_t textSize = 1) {
  tft.fillRect(x, y, w, h, C_BG);
  tft.setFont();
  tft.setTextSize(textSize);
  tft.setTextColor(C_TEXT);
  String shown = text.substring(0, min(charsToShow, int(text.length())));
  int lineH = 13 * textSize;
  int maxChars = max(5, w / (6 * textSize));
  int visibleLines = max(1, h / lineH);
  String lines[32];
  int lineCount = wrapText(shown, lines, 32, maxChars);
  int first = startLine >= 0 ? startLine : max(0, lineCount - visibleLines);
  first = constrain(first, 0, max(0, lineCount - 1));
  for (int row = 0; row < visibleLines && first + row < lineCount; row++) {
    tft.setCursor(x, y + row * lineH);
    tft.print(lines[first + row]);
  }
}

static void drawHeader() {
  String header = String(WiFi.isConnected() ? "WIFI" : "OFFLINE") + "  WEB:" + (webpageOnline ? "OK" : "--") + "  STT:" + (whisperOnline ? "OK" : "--");
  if (header == prevHeader) return;
  tft.fillRect(0, HEADER_Y, W, HEADER_H, C_SURFACE);
  drawText(9, 7, "NIXIE", C_TEXT, 2);
  drawText(90, 9, header, C_MUTED, 1);
  prevHeader = header;
}

static void drawStatusPill() {
  String status = String(statusWord(currentState)) + " " + fitText(currentStatus, 35);
  if (currentState == prevState && status == prevStatus) return;
  tft.fillRect(8, STATUS_Y, 304, STATUS_H, C_BG);
  tft.fillRoundRect(8, STATUS_Y, 304, STATUS_H, 5, C_SURFACE_2);
  tft.fillRoundRect(8, STATUS_Y, 54, STATUS_H, 5, statusColor(currentState));
  drawText(18, STATUS_Y + 5, statusWord(currentState), C_BG, 1);
  drawText(70, STATUS_Y + 5, fitText(currentStatus, 38), C_TEXT, 1);
  prevState = currentState;
  prevStatus = status;
}

static void drawAssistantStateBadge() {
  const char* label = "READY";
  uint16_t fill = C_BLUE;
  uint16_t textColor = C_BG;

  if (currentState == StatusState::Recording) {
    label = "LISTEN";
    fill = C_GREEN;
  } else if (currentState == StatusState::Sending) {
    label = "PROC";
    fill = C_AMBER;
  }

  const int badgeW = 54;
  const int badgeH = 15;
  const int badgeX = 248;
  const int badgeY = STATUS_Y + 1;
  tft.fillRoundRect(badgeX, badgeY, badgeW, badgeH, 5, fill);
  tft.drawRoundRect(badgeX, badgeY, badgeW, badgeH, 5, C_LINE);
  drawText(badgeX + 8, badgeY + 4, label, textColor, 1);
}

static void drawNav() {
  if (selectedMenu == prevMenu) return;
  tft.fillRect(0, NAV_Y, W, NAV_H, C_SURFACE);
  int x = 3;
  for (int i = 0; i < MENU_COUNT; i++) {
    int bw = 36;
    uint16_t fill = (i == selectedMenu) ? C_BLUE : C_SURFACE_2;
    uint16_t text = (i == selectedMenu) ? C_BG : C_MUTED;
    tft.fillRoundRect(x, NAV_Y + 5, bw, 17, 4, fill);
    tft.drawRoundRect(x, NAV_Y + 5, bw, 17, 4, C_LINE);
    drawText(x + 5, NAV_Y + 10, menuNames[i], text, 1);
    x += bw + 4;
  }
  prevMenu = selectedMenu;
}

static void drawBigClock() {
  String nowText = clockNow();
  if (nowText == prevClock) return;
  tft.fillRect(18, 69, 284, 56, C_BG);
  drawText(30, 72, nowText, C_TEXT, 5);
  prevClock = nowText;
}

static void drawHomeFrame() {
  tft.fillScreen(C_BG);
  drawHeader();
  drawStatusPill();
  drawText(18, 55, "Clock", C_MUTED, 1);
  tft.drawRoundRect(16, 66, 288, 66, 7, C_LINE);
  
  // Timer badge
  tft.fillRoundRect(18, 145, 62, 40, 6, C_SURFACE_2);
  drawText(20, 147, "TIMER", C_MUTED, 1);
  
  // Pomodoro badge
  tft.fillRoundRect(87, 145, 62, 40, 6, C_SURFACE_2);
  drawText(93, 147, "POMO", C_MUTED, 1);
  
  // Weather badge
  tft.fillRoundRect(156, 145, 142, 40, 6, C_SURFACE_2);
  drawText(158, 147, "Weather", C_MUTED, 1);
  
  drawNav();
  resetDirtyCache();
  prevScreen = ScreenMode::Home;
}

static void updateHome() {
  drawHeader();
  drawStatusPill();
  drawBigClock();
  
  // Update Timer badge
  char timerBuf[12];
  snprintf(timerBuf, sizeof(timerBuf), "%02d:%02d", timerRemaining / 60, timerRemaining % 60);
  tft.fillRect(20, 160, 58, 20, C_SURFACE_2);
  drawText(22, 163, timerRemaining > 0 ? timerBuf : "idle", timerRemaining > 0 ? C_RED : C_MUTED, 1);
  
  // Update Pomodoro badge
  char pomoBuf[12];
  snprintf(pomoBuf, sizeof(pomoBuf), "%02d:%02d", pomodoroRemaining / 60, pomodoroRemaining % 60);
  tft.fillRect(89, 160, 58, 20, C_SURFACE_2);
  uint16_t pomoColor = pomodoroRunning ? (pomodoroBreak ? C_GREEN : C_AMBER) : C_MUTED;
  drawText(91, 163, pomodoroRunning ? pomoBuf : "idle", pomoColor, 1);
  
  // Update Weather badge
  tft.fillRect(158, 160, 138, 20, C_SURFACE_2);
  drawText(160, 163, fitText(weatherLine1, 22), C_TEXT, 1);
  
  drawNav();
}

static void drawArc(int cx, int cy, int r, int thickness, float fraction, uint16_t color) {
  fraction = constrain(fraction, 0.0f, 1.0f);
  for (int a = -90; a < 270; a += 4) {
    float rad = a * 0.0174533f;
    int x1 = cx + cos(rad) * (r - thickness);
    int y1 = cy + sin(rad) * (r - thickness);
    int x2 = cx + cos(rad) * r;
    int y2 = cy + sin(rad) * r;
    tft.drawLine(x1, y1, x2, y2, C_LINE);
  }
  int endA = -90 + int(360.0f * fraction);
  for (int a = -90; a < endA; a += 4) {
    float rad = a * 0.0174533f;
    int x1 = cx + cos(rad) * (r - thickness);
    int y1 = cy + sin(rad) * (r - thickness);
    int x2 = cx + cos(rad) * r;
    int y2 = cy + sin(rad) * r;
    tft.drawLine(x1, y1, x2, y2, color);
  }
}

static void drawSolidDial(int cx, int cy, int r, int inner, float fraction, uint16_t fillColor, uint16_t emptyColor) {
  fraction = constrain(fraction, 0.0f, 1.0f);
  int activeEnd = -90 + int(360.0f * fraction);
  for (int a = -90; a < 270; a += 2) {
    float rad = a * 0.0174533f;
    int x1 = cx + cos(rad) * inner;
    int y1 = cy + sin(rad) * inner;
    int x2 = cx + cos(rad) * r;
    int y2 = cy + sin(rad) * r;
    tft.drawLine(x1, y1, x2, y2, a < activeEnd ? fillColor : emptyColor);
  }
  tft.fillCircle(cx, cy, inner - 2, C_BG);
  tft.drawCircle(cx, cy, r, C_LINE);
  tft.drawCircle(cx, cy, inner, C_LINE);
}

static void drawTimerFrame() {
  tft.fillScreen(C_BG);
  drawHeader();
  drawStatusPill();
  drawText(18, 55, "Timer", C_MUTED, 1);
  drawText(195, 55, "rotate set, press start", C_MUTED, 1);
  drawNav();
  resetDirtyCache();
  prevScreen = ScreenMode::Timer;
}

static void updateTimerScreen() {
  drawHeader();
  drawStatusPill();
  int shownRemaining = manualTimerRunning ? manualTimerSeconds : (timerRemaining > 0 ? timerRemaining : manualTimerSeconds);
  int shownTotal = manualTimerRunning ? manualTimerTotal : (timerRemaining > 0 ? timerTotal : manualTimerTotal);
  bool running = manualTimerRunning || timerRemaining > 0;
  if (shownRemaining != prevManualTimerSeconds || shownTotal != prevManualTimerTotal || running != prevManualTimerRunning) {
    tft.fillRect(34, 65, 252, 132, C_BG);
    int total = max(shownTotal, shownRemaining);
    float f = total > 0 ? (float)shownRemaining / (float)total : 0.0f;
    drawSolidDial(160, 126, 61, 42, f, C_RED, C_SURFACE_2);
    int h = shownRemaining / 3600;
    int m = (shownRemaining % 3600) / 60;
    int s = shownRemaining % 60;
    char buf[18];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d", h, m, s);
    drawText(73, 109, buf, running ? C_TEXT : C_MUTED, 3);
    drawText(92, 163, running ? "solid countdown dial" : "rotate knob to set", running ? C_RED : C_MUTED, 1);
    prevManualTimerSeconds = shownRemaining;
    prevManualTimerTotal = shownTotal;
    prevManualTimerRunning = running;
  }
  drawNav();
}

static void drawPomodoroFrame() {
  tft.fillScreen(C_BG);
  drawHeader();
  drawStatusPill();
  drawText(18, 55, "Pomodoro", C_MUTED, 1);
  drawText(211, 55, "press to start/pause", C_MUTED, 1);
  drawNav();
  resetDirtyCache();
  prevScreen = ScreenMode::Pomodoro;
}

static void updatePomodoroScreen() {
  drawHeader();
  drawStatusPill();
  if (pomodoroRemaining != prevPomodoroRemaining || pomodoroRunning != prevPomodoroRunning || pomodoroBreak != prevPomodoroBreak) {
    tft.fillRect(28, 67, 264, 132, C_BG);
    float total = pomodoroBreak ? 5.0f * 60.0f : 25.0f * 60.0f;
    float f = constrain((float)pomodoroRemaining / total, 0.0f, 1.0f);
    drawArc(160, 128, 58, 12, f, pomodoroBreak ? C_GREEN : C_AMBER);
    char buf[12];
    snprintf(buf, sizeof(buf), "%02d:%02d", pomodoroRemaining / 60, pomodoroRemaining % 60);
    drawText(94, 112, buf, C_TEXT, 4);
    drawText(109, 158, pomodoroBreak ? "short break" : "focus block", pomodoroBreak ? C_GREEN : C_AMBER, 1);
    drawText(112, 174, pomodoroRunning ? "running" : "paused", pomodoroRunning ? C_GREEN : C_MUTED, 1);
    prevPomodoroRemaining = pomodoroRemaining;
    prevPomodoroRunning = pomodoroRunning;
    prevPomodoroBreak = pomodoroBreak;
  }
  drawNav();
}

static void drawFocusFrame() {
  tft.fillScreen(C_BG);
  drawHeader();
  drawStatusPill();
  drawText(18, 55, "Focus Mode", C_MUTED, 1);
  drawText(165, 55, "press to start", C_MUTED, 1);
  tft.drawRoundRect(16, 70, 288, 135, 7, C_LINE);
  drawNav();
  resetDirtyCache();
  prevScreen = ScreenMode::Focus;
}

static void updateFocusScreen() {
  drawHeader();
  drawStatusPill();
  
  if (focusElapsed != prevFocusElapsed || focusRunning != prevFocusRunning || focusHistoryCount != prevFocusHistoryCount) {
    // Clear content area
    tft.fillRect(18, 72, 284, 131, C_BG);
    
    // Draw current focus session
    tft.drawRoundRect(20, 74, 280, 70, 6, focusRunning ? C_BLUE : C_SURFACE_2);
    int min = focusElapsed / 60;
    int sec = focusElapsed % 60;
    char focusBuf[16];
    snprintf(focusBuf, sizeof(focusBuf), "%02d:%02d", min, sec);
    drawText(50, 84, "Current session", focusRunning ? C_BLUE : C_MUTED, 1);
    drawText(80, 102, focusBuf, focusRunning ? C_GREEN : C_MUTED, 3);
    drawText(140, 155, focusRunning ? "RUNNING" : "IDLE", focusRunning ? C_GREEN : C_MUTED, 1);
    
    // Draw history
    drawText(24, 157, "History (last 4):", C_MUTED, 1);
    int historyRow = 0;
    for (int i = max(0, focusHistoryCount - 4); i < focusHistoryCount && historyRow < 4; i++, historyRow++) {
      int hMin = focusHistory[i] / 60;
      int hSec = focusHistory[i] % 60;
      char histBuf[20];
      snprintf(histBuf, sizeof(histBuf), "  %d. %02d:%02d", historyRow + 1, hMin, hSec);
      drawText(24, 169 + historyRow * 12, histBuf, C_TEXT, 1);
    }
    
    prevFocusElapsed = focusElapsed;
    prevFocusRunning = focusRunning;
    prevFocusHistoryCount = focusHistoryCount;
  }
  
  drawNav();
}

static void drawDecisionFrame() {
  tft.fillScreen(C_BG);
  drawHeader();
  drawStatusPill();
  drawText(18, 55, "Decision spinner", C_MUTED, 1);
  drawText(201, 55, "press to spin", C_MUTED, 1);
  drawNav();
  resetDirtyCache();
  prevScreen = ScreenMode::Decision;
}

static void updateDecisionScreen() {
  drawHeader();
  drawStatusPill();
  if (decisionYes != prevDecisionYes || decisionSpinning != prevDecisionSpinning || decisionFrame != prevDecisionFrame) {
    tft.fillRect(18, 70, 284, 132, C_BG);
    tft.drawCircle(160, 132, 54, C_LINE);
    tft.drawCircle(160, 132, 55, C_LINE);
    int angle = decisionSpinning ? (decisionFrame * 29) % 360 : (decisionYes ? 28 : 208);
    float rad = angle * 0.0174533f;
    tft.drawLine(160, 132, 160 + cos(rad) * 46, 132 + sin(rad) * 46, decisionYes ? C_GREEN : C_RED);
    tft.fillRoundRect(56, 112, 82, 40, 8, decisionYes && !decisionSpinning ? C_GREEN : C_SURFACE_2);
    tft.fillRoundRect(182, 112, 82, 40, 8, !decisionYes && !decisionSpinning ? C_RED : C_SURFACE_2);
    drawText(83, 126, "YES", decisionYes && !decisionSpinning ? C_BG : C_TEXT, 2);
    drawText(215, 126, "NO", !decisionYes && !decisionSpinning ? C_BG : C_TEXT, 2);
    drawText(83, 177, decisionSpinning ? "release button to stop" : "hold button to spin", C_MUTED, 1);
    prevDecisionYes = decisionYes;
    prevDecisionSpinning = decisionSpinning;
    prevDecisionFrame = decisionFrame;
  }
  drawNav();
}

static void drawCalendarFrame() {
  tft.fillScreen(C_BG);
  drawHeader();
  drawStatusPill();
  drawText(18, 55, "Calendar", C_MUTED, 1);
  drawNav();
  resetDirtyCache();
  prevScreen = ScreenMode::Calendar;
}

static void updateCalendarScreen() {
  drawHeader();
  drawStatusPill();
  struct tm now;
  char key[20] = "--";
  if (getLocalTime(&now, 10)) {
    snprintf(key, sizeof(key), "%04d-%02d-%02d", now.tm_year + 1900, now.tm_mon + 1, now.tm_mday);
  }
  String dayKey(key);
  if (dayKey != prevCalendarDay) {
    tft.fillRect(18, 70, 284, 132, C_BG);
    if (!getLocalTime(&now, 10)) {
      drawText(74, 124, "Calendar needs clock sync", C_MUTED, 1);
    } else {
      const char* months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
      char title[28];
      snprintf(title, sizeof(title), "%s %d", months[now.tm_mon], now.tm_year + 1900);
      drawText(112, 73, title, C_TEXT, 2);
      drawText(45, 99, "S  M  T  W  T  F  S", C_MUTED, 1);
      struct tm first = now;
      first.tm_mday = 1;
      mktime(&first);
      int daysInMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
      int year = now.tm_year + 1900;
      if ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0) daysInMonth[1] = 29;
      int d = 1;
      for (int row = 0; row < 6; row++) {
        for (int col = 0; col < 7; col++) {
          int slot = row * 7 + col;
          if (slot < first.tm_wday || d > daysInMonth[now.tm_mon]) continue;
          int x = 45 + col * 33;
          int y = 116 + row * 14;
          if (d == now.tm_mday) {
            tft.fillRoundRect(x - 4, y - 3, 22, 12, 4, C_BLUE);
            drawText(x, y, String(d), C_BG, 1);
          } else {
            drawText(x, y, String(d), C_TEXT, 1);
          }
          d++;
        }
      }
      drawText(92, 196, fitText(scheduleLine3, 22), C_GREEN, 1);
    }
    prevCalendarDay = dayKey;
  }
  drawNav();
}

static void drawWeatherFrame() {
  tft.fillScreen(C_BG);
  drawHeader();
  drawStatusPill();
  drawText(18, 55, "Weather", C_MUTED, 1);
  tft.fillCircle(66, 113, 28, C_AMBER);
  tft.fillCircle(112, 127, 22, C_SURFACE_2);
  tft.fillRoundRect(80, 125, 74, 24, 12, C_SURFACE_2);
  drawNav();
  resetDirtyCache();
  prevScreen = ScreenMode::Weather;
}

static void updateWeatherScreen() {
  drawHeader();
  drawStatusPill();
  if (weatherLine1 != prevWeather1 || weatherLine2 != prevWeather2) {
    tft.fillRect(174, 89, 128, 76, C_BG);
    drawText(174, 93, fitText(weatherLine1, 18), C_TEXT, 2);
    drawText(176, 130, fitText(weatherLine2, 24), C_MUTED, 1);
    prevWeather1 = weatherLine1;
    prevWeather2 = weatherLine2;
  }
  drawNav();
}

static void drawScheduleFrame() {
  tft.fillScreen(C_BG);
  drawHeader();
  drawStatusPill();
  drawText(18, 55, "Reminders, alarms, events", C_MUTED, 1);
  tft.drawRoundRect(18, 72, 284, 38, 7, C_LINE);
  tft.drawRoundRect(18, 118, 284, 34, 7, C_LINE);
  tft.drawRoundRect(18, 160, 284, 34, 7, C_LINE);
  drawText(30, 84, "Alarm", C_AMBER, 1);
  drawText(30, 129, "Remind", C_CYAN, 1);
  drawText(30, 171, "Event", C_GREEN, 1);
  drawNav();
  resetDirtyCache();
  prevScreen = ScreenMode::Schedule;
}

static void updateScheduleScreen() {
  drawHeader();
  drawStatusPill();
  if (scheduleLine1 != prevSchedule1) {
    tft.fillRect(96, 84, 190, 16, C_BG);
    drawText(98, 84, fitText(scheduleLine1, 30), C_TEXT, 1);
    prevSchedule1 = scheduleLine1;
  }
  if (scheduleLine2 != prevSchedule2) {
    tft.fillRect(96, 129, 190, 16, C_BG);
    drawText(98, 129, fitText(scheduleLine2, 30), C_TEXT, 1);
    prevSchedule2 = scheduleLine2;
  }
  if (scheduleLine3 != prevSchedule3) {
    tft.fillRect(96, 171, 190, 16, C_BG);
    drawText(98, 171, fitText(scheduleLine3, 30), C_TEXT, 1);
    prevSchedule3 = scheduleLine3;
  }
  drawNav();
}

static void drawMicIcon(int cx, int cy, uint16_t color) {
  tft.fillRoundRect(cx - 5, cy - 13, 10, 20, 5, color);
  tft.drawRoundRect(cx - 5, cy - 13, 10, 20, 5, C_TEXT);
  tft.drawLine(cx - 10, cy - 2, cx - 10, cy + 3, color);
  tft.drawLine(cx + 10, cy - 2, cx + 10, cy + 3, color);
  tft.drawLine(cx - 10, cy + 3, cx, cy + 10, color);
  tft.drawLine(cx + 10, cy + 3, cx, cy + 10, color);
  tft.drawLine(cx, cy + 10, cx, cy + 18, color);
  tft.drawLine(cx - 8, cy + 18, cx + 8, cy + 18, color);
}

static float graphValue(const String& expr, float x) {
  String e = expr;
  e.toLowerCase();
  e.replace(" ", "");
  e.replace("y=", "");
  e.replace("taninverse", "atan");
  e.replace("arctan", "atan");
  e.replace("sininverse", "asin");
  e.replace("arcsin", "asin");
  e.replace("cosinverse", "acos");
  e.replace("arccos", "acos");
  e.replace("-", "+-");
  float y = 0.0f;
  bool matched = false;
  int start = 0;
  while (start < e.length()) {
    int plus = e.indexOf('+', start);
    String term = plus >= 0 ? e.substring(start, plus) : e.substring(start);
    term.trim();
    start = plus >= 0 ? plus + 1 : e.length();
    if (term.length() == 0) continue;

    float sign = 1.0f;
    if (term[0] == '-') {
      sign = -1.0f;
      term = term.substring(1);
    }

    if (term.indexOf("atan") >= 0) {
      y += sign * atan(x);
      matched = true;
    } else if (term.indexOf("asin") >= 0) {
      y += sign * asin(constrain(x, -1.0f, 1.0f));
      matched = true;
    } else if (term.indexOf("acos") >= 0) {
      y += sign * acos(constrain(x, -1.0f, 1.0f));
      matched = true;
    } else if (term.indexOf("sin") >= 0) {
      y += sign * sin(x);
      matched = true;
    } else if (term.indexOf("cos") >= 0) {
      y += sign * cos(x);
      matched = true;
    } else if (term.indexOf("tan") >= 0) {
      y += sign * constrain(tan(x), -3.0f, 3.0f);
      matched = true;
    } else if (term.indexOf("sqrt") >= 0) {
      y += sign * sqrt(fabs(x));
      matched = true;
    } else if (term.indexOf("x^3") >= 0) {
      String coeff = term.substring(0, term.indexOf("x^3"));
      coeff.replace("*", "");
      float c = coeff.length() ? coeff.toFloat() : 1.0f;
      y += sign * c * x * x * x;
      matched = true;
    } else if (term.indexOf("x^2") >= 0 || term.indexOf("x*x") >= 0) {
      int idx = term.indexOf("x^2");
      if (idx < 0) idx = term.indexOf("x*x");
      String coeff = term.substring(0, idx);
      coeff.replace("*", "");
      float c = coeff.length() ? coeff.toFloat() : 1.0f;
      y += sign * c * x * x;
      matched = true;
    } else if (term.indexOf('x') >= 0) {
      String coeff = term.substring(0, term.indexOf('x'));
      coeff.replace("*", "");
      float c = coeff.length() ? coeff.toFloat() : 1.0f;
      y += sign * c * x;
      matched = true;
    } else {
      y += sign * term.toFloat();
    }
  }
  return matched ? y : e.toFloat();
}

static void drawMathPanel(int16_t x, int16_t y, int16_t w, int16_t h) {
  if (!mathVisible) return;
  tft.fillRoundRect(x, y, w, h, 7, C_SURFACE_2);
  tft.drawRoundRect(x, y, w, h, 7, graphVisible ? C_CYAN : C_AMBER);
  drawText(x + 8, y + 8, graphVisible ? "Graph" : "Math", graphVisible ? C_CYAN : C_AMBER, 1);
  drawText(x + 8, y + 21, fitText(mathExpr, 13), C_TEXT, 1);
  drawText(x + 8, y + 34, fitText(mathResult, 13), C_MUTED, 1);
  if (!graphVisible) return;

  int gx = x + 66;
  int gy = y + 8;
  int gw = w - 76;
  int gh = h - 16;
  tft.fillRect(gx, gy, gw, gh, ILI9341_WHITE);
  tft.drawRect(gx, gy, gw, gh, ILI9341_BLACK);
  for (int px = gx + 1; px < gx + gw; px += max(8, gw / 8)) {
    tft.drawFastVLine(px, gy, gh, 0xC618);
  }
  for (int py = gy + 1; py < gy + gh; py += max(8, gh / 8)) {
    tft.drawFastHLine(gx, py, gw, 0xC618);
  }
  tft.drawFastHLine(gx, gy + gh / 2, gw, ILI9341_BLACK);
  tft.drawFastVLine(gx + gw / 2, gy, gh, ILI9341_BLACK);
  int lastX = gx;
  int lastY = gy + gh / 2;
  for (int px = 0; px < gw; px++) {
    float xx = -3.0f + 6.0f * px / max(1, gw - 1);
    float yy = graphValue(mathExpr, xx);
    yy = constrain(yy, -3.0f, 3.0f);
    int sy = gy + gh / 2 - int((yy / 3.0f) * (gh / 2 - 2));
    if (px > 0) tft.drawLine(lastX, lastY, gx + px, sy, ILI9341_RED);
    lastX = gx + px;
    lastY = sy;
  }
}

static void drawAssistantFrame() {
  tft.fillScreen(C_BG);
  drawHeader();
  drawStatusPill();
  drawAssistantStateBadge();
  drawText(18, 55, "Assistant", C_MUTED, 1);
  tft.drawRoundRect(12, 69, 296, 139, 10, C_LINE);
  tft.drawRoundRect(20, 80, 280, 36, 6, C_LINE);
  tft.drawRoundRect(20, 122, 280, 78, 6, C_LINE);
  drawText(28, 85, "You", C_MUTED, 1);
  drawText(28, 127, "Reply", C_MUTED, 1);
  drawNav();
  resetDirtyCache();
  prevScreen = ScreenMode::Assistant;
}

static void updateAssistantScreen() {
  drawHeader();
  drawStatusPill();
  drawAssistantStateBadge();

  if (millis() - lastTranscriptAnimMs >= 48) {
    bool advanced = false;
    if (transcriptionChars < lastTranscription.length()) {
      transcriptionChars++;
      advanced = true;
    }
    if (typewriterChars < lastOutput.length()) {
      typewriterChars++;
      advanced = true;
    }
    if (advanced) {
      lastTranscriptAnimMs = millis();
    }
  }

  if (lastTranscription != prevTranscription || transcriptionChars != prevTranscriptionChars) {
    tft.fillRect(26, 94, 268, 18, C_BG);
    renderWrappedLines(26, 94, 268, 18, lastTranscription, transcriptionChars, -1, 1);
    prevTranscription = lastTranscription;
    prevTranscriptionChars = transcriptionChars;
  }
  bool mathChanged = mathVisible != prevMathVisible || graphVisible != prevGraphVisible || mathExpr != prevMathExpr || mathResult != prevMathResult;
  if (lastOutput != prevOutput || typewriterChars != prevTypewriterChars || mathChanged) {
    if (mathVisible && graphVisible) {
      tft.fillRect(26, 140, 268, 56, C_BG);
      int lineH = 13;
      int maxChars = 45;
      int visibleLines = 3;
      String shown = lastOutput.substring(0, typewriterChars);
      String lines[32];
      int lineCount = wrapText(shown, lines, 32, maxChars);
      int startLine = max(0, lineCount - visibleLines);
      tft.setFont();
      tft.setTextSize(1);
      tft.setTextColor(C_TEXT);
      for (int row = 0; row < visibleLines && startLine + row < lineCount; row++) {
        tft.setCursor(26, 140 + row * lineH);
        tft.print(lines[startLine + row]);
      }
      drawMathPanel(58, 126, 204, 80);
    } else if (mathVisible) {
      int replyH = 18;
      tft.fillRect(26, 140, 268, replyH + 2, C_BG);
      renderWrappedLines(26, 140, 268, replyH, lastOutput, typewriterChars, -1, 1);
      drawMathPanel(24, 172, 272, 34);
    } else {
      tft.fillRect(26, 140, 268, 56, C_BG);
      renderWrappedLines(26, 140, 268, 56, lastOutput, typewriterChars, -1, 1);
    }
    prevOutput = lastOutput;
    prevTypewriterChars = typewriterChars;
    prevMathExpr = mathExpr;
    prevMathResult = mathResult;
    prevMathVisible = mathVisible;
    prevGraphVisible = graphVisible;
  }
  drawNav();
}

static void drawCurrentScreen() {
  switch (currentScreen) {
    case ScreenMode::Timer: drawTimerFrame(); break;
    case ScreenMode::Pomodoro: drawPomodoroFrame(); break;
    case ScreenMode::Focus: drawFocusFrame(); break;
    case ScreenMode::Decision: drawDecisionFrame(); break;
    case ScreenMode::Calendar: drawCalendarFrame(); break;
    case ScreenMode::Weather: drawWeatherFrame(); break;
    case ScreenMode::Schedule: drawScheduleFrame(); break;
    case ScreenMode::Assistant: drawAssistantFrame(); break;
    case ScreenMode::Home:
    default: drawHomeFrame(); break;
  }
  screenDrawn = true;
}

static void refreshDynamic() {
  if (!display_ok) return;
  if (!screenDrawn || currentScreen != prevScreen) drawCurrentScreen();
  switch (currentScreen) {
    case ScreenMode::Timer: updateTimerScreen(); break;
    case ScreenMode::Pomodoro: updatePomodoroScreen(); break;
    case ScreenMode::Focus: updateFocusScreen(); break;
    case ScreenMode::Decision: updateDecisionScreen(); break;
    case ScreenMode::Calendar: updateCalendarScreen(); break;
    case ScreenMode::Weather: updateWeatherScreen(); break;
    case ScreenMode::Schedule: updateScheduleScreen(); break;
    case ScreenMode::Assistant: updateAssistantScreen(); break;
    case ScreenMode::Home:
    default: updateHome(); break;
  }
}

void displayInit() {
  pinMode(TFT_LED_PIN, OUTPUT);
  digitalWrite(TFT_LED_PIN, HIGH);
  SPI.begin(TFT_SCK_PIN, TFT_MISO_PIN, TFT_MOSI_PIN, TFT_CS_PIN);
  tft.begin();
  tft.setRotation(1);
  display_ok = true;
  screenDrawn = false;
  tft.fillScreen(C_BG);
}

void displayBootAnimation() {
  if (!display_ok) return;
  screenDrawn = false;
  tft.fillScreen(ILI9341_BLACK);
  for (int y = 0; y < H; y += 4) {
    tft.drawFastHLine(0, y, W, 0x1082);
  }
  tft.drawRoundRect(36, 32, 248, 162, 9, C_BLUE);
  tft.drawRoundRect(39, 35, 242, 156, 7, C_ORANGE);
  drawText(88, 48, "NIXIE // voice shell", C_CYAN, 1);

  tft.setFont(&FreeSerifItalic24pt7b);
  const char* word = "NIXIE";
  for (int i = 0; i < 5; i++) {
    for (int glow = 2; glow >= 0; glow--) {
      uint16_t c = glow ? blend565(C_ORANGE, C_BLUE, i * 58) : C_TEXT;
      tft.setTextColor(c);
      tft.setCursor(72 + i * 35 + glow, 121);
      tft.print(word[i]);
    }
    delay(140);
  }
  for (int y = 78; y < 136; y += 5) {
    tft.drawFastHLine(55, y, 218, 0x0841);
  }
  tft.setFont();
  drawText(99, 147, "neural desk assistant", C_MUTED, 1);
  tft.drawRect(74, 164, 172, 10, C_LINE);
  for (int i = 0; i <= 168; i += 12) {
    tft.fillRect(76, 164, i, 6, blend565(C_ORANGE, C_BLUE, i));
    delay(35);
  }
}

void displayShowWifiConnecting(const char* ssid) {
  if (!display_ok) return;
  screenDrawn = false;
  tft.fillScreen(C_BG);
  drawText(54, 72, "Connecting WiFi", C_TEXT, 2);
  drawText(86, 108, ssid ? ssid : "network", C_MUTED, 1);
  tft.drawRoundRect(58, 142, 204, 12, 6, C_LINE);
  for (int i = 0; i < 5; i++) {
    tft.fillRoundRect(60, 144, 36 + i * 34, 8, 4, C_BLUE);
    delay(120);
  }
}

void displayShowWifiResult(bool connected, const String& detail) {
  if (!display_ok) return;
  screenDrawn = false;
  tft.fillScreen(C_BG);
  drawText(58, 82, connected ? "WiFi connected" : "WiFi failed", connected ? C_GREEN : C_RED, 2);
  drawText(54, 122, fitText(detail, 34), C_MUTED, 1);
  delay(800);
}

void displayShowDashboard() {
  currentScreen = ScreenMode::Home;
  selectedMenu = 0;
  screenDrawn = false;
  refreshDynamic();
}

static ScreenMode screenForMenu(int menu) {
  switch (menu) {
    case 1: return ScreenMode::Timer;
    case 2: return ScreenMode::Pomodoro;
    case 3: return ScreenMode::Focus;
    case 4: return ScreenMode::Decision;
    case 5: return ScreenMode::Calendar;
    case 6: return ScreenMode::Weather;
    case 7: return ScreenMode::Schedule;
    case 8: return ScreenMode::Assistant;
    case 0:
    default: return ScreenMode::Home;
  }
}

static void setScreenForMenu(int menu) {
  selectedMenu = (menu + MENU_COUNT) % MENU_COUNT;
  currentScreen = screenForMenu(selectedMenu);
  screenDrawn = false;
}

void displayNavigateMenu(int delta) {
  if (screenActive && currentScreen == ScreenMode::Timer && !manualTimerRunning && timerRemaining == 0) {
    manualTimerSeconds = constrain(manualTimerSeconds + delta * 60, 60, 99 * 60);
    manualTimerTotal = manualTimerSeconds;
    currentStatus = "Timer set";
    prevManualTimerSeconds = -1;
    refreshDynamic();
    return;
  }
  if (screenActive && currentScreen == ScreenMode::Decision && decisionSpinning) return;
  screenActive = false;
  setScreenForMenu(selectedMenu + delta);
  currentStatus = String("Select ") + menuNames[selectedMenu];
  refreshDynamic();
}

void displaySelectMenu() {
  if (!screenActive) {
    currentScreen = screenForMenu(selectedMenu);
    screenActive = currentScreen == ScreenMode::Timer || currentScreen == ScreenMode::Pomodoro || currentScreen == ScreenMode::Focus || currentScreen == ScreenMode::Decision;
    currentStatus = String(menuNames[selectedMenu]);
    if (currentScreen == ScreenMode::Decision) {
      decisionSpinning = true;
      currentStatus = "Release to stop";
    }
    screenDrawn = false;
    refreshDynamic();
    return;
  }

  if (currentScreen == ScreenMode::Timer) {
    manualTimerRunning = !manualTimerRunning;
    if (manualTimerRunning) {
      manualTimerTotal = max(manualTimerTotal, manualTimerSeconds);
      manualTimerEndMs = millis() + (unsigned long)manualTimerSeconds * 1000UL;
      currentStatus = "Timer running";
    } else {
      currentStatus = "Timer paused";
    }
  } else if (currentScreen == ScreenMode::Pomodoro) {
    pomodoroRunning = !pomodoroRunning;
    if (pomodoroRunning) {
      pomodoroEndMs = millis() + (unsigned long)pomodoroRemaining * 1000UL;
      currentStatus = "Pomodoro running";
    } else {
      currentStatus = "Pomodoro paused";
    }
  } else if (currentScreen == ScreenMode::Focus) {
    if (!focusRunning) {
      focusRunning = true;
      focusElapsed = 0;
      focusStartMs = millis();
      currentStatus = "Focus running";
    } else {
      focusRunning = false;
      if (focusHistoryCount < MAX_FOCUS_HISTORY) {
        focusHistory[focusHistoryCount++] = focusElapsed;
      } else {
        for (int i = 1; i < MAX_FOCUS_HISTORY; i++) {
          focusHistory[i - 1] = focusHistory[i];
        }
        focusHistory[MAX_FOCUS_HISTORY - 1] = focusElapsed;
      }
      currentStatus = "Focus saved";
    }
  } else if (currentScreen == ScreenMode::Decision) {
    decisionSpinning = true;
    decisionSpinUntilMs = 0;
    currentStatus = "Release to stop";
  } else {
    currentStatus = String(menuNames[selectedMenu]);
    screenActive = false;
  }
  refreshDynamic();
}

void displayReleaseMenu() {
  if (screenActive && currentScreen == ScreenMode::Decision && decisionSpinning) {
    decisionSpinning = false;
    decisionYes = ((millis() / 173) % 2) == 0;
    currentStatus = decisionYes ? "Decision: yes" : "Decision: no";
    refreshDynamic();
  }
}

void displayUiTick() {
  static unsigned long lastUiMs = 0;
  unsigned long now = millis();
  bool changed = false;

  if (pomodoroRunning) {
    long remaining = (long)((pomodoroEndMs - now) / 1000UL);
    if (remaining <= 0) {
      pomodoroBreak = !pomodoroBreak;
      pomodoroRemaining = pomodoroBreak ? 5 * 60 : 25 * 60;
      pomodoroEndMs = now + (unsigned long)pomodoroRemaining * 1000UL;
      currentStatus = pomodoroBreak ? "Break started" : "Focus started";
      changed = true;
    } else if (remaining != pomodoroRemaining) {
      pomodoroRemaining = remaining;
      changed = true;
    }
  }

  if (manualTimerRunning) {
    long remaining = (long)((manualTimerEndMs - now) / 1000UL);
    if (remaining <= 0) {
      manualTimerRunning = false;
      manualTimerSeconds = manualTimerTotal;
      currentStatus = "Timer done";
      changed = true;
    } else if (remaining != manualTimerSeconds) {
      manualTimerSeconds = remaining;
      changed = true;
    }
  }

  if (focusRunning) {
    int elapsed = (int)((now - focusStartMs) / 1000UL);
    if (elapsed != focusElapsed) {
      focusElapsed = elapsed;
      changed = true;
    }
  }

  if (decisionSpinning && now - lastUiMs >= 80) {
    decisionFrame++;
    decisionYes = (decisionFrame % 2) == 0;
    if (decisionSpinUntilMs > 0 && now >= decisionSpinUntilMs) {
      decisionSpinning = false;
      decisionYes = ((now / 173) % 2) == 0;
      currentStatus = decisionYes ? "Decision: yes" : "Decision: no";
    }
    changed = true;
  }

  if (now - lastUiMs >= 1000) {
    changed = true;
  }

  if (currentScreen == ScreenMode::Assistant &&
      (transcriptionChars < lastTranscription.length() || typewriterChars < lastOutput.length()) &&
      (now - lastTranscriptAnimMs) >= 48) {
    changed = true;
  }

  if (changed) refreshDynamic();
  if (now - lastUiMs >= 80) lastUiMs = now;
}

void displayOpenTimer() {
  selectedMenu = 1;
  currentScreen = ScreenMode::Timer;
  screenActive = true;
  currentStatus = "Timer";
  screenDrawn = false;
  refreshDynamic();
}

void displayOpenPomodoro(bool startNow) {
  selectedMenu = 2;
  currentScreen = ScreenMode::Pomodoro;
  screenActive = true;
  if (startNow && !pomodoroRunning) {
    pomodoroRunning = true;
    pomodoroEndMs = millis() + (unsigned long)pomodoroRemaining * 1000UL;
    currentStatus = "Pomodoro running";
  } else {
    currentStatus = "Pomodoro";
  }
  screenDrawn = false;
  refreshDynamic();
}

void displaySetServiceStatus(bool webpage, bool whisper) {
  webpageOnline = webpage;
  whisperOnline = whisper;
  prevHeader = "";
  refreshDynamic();
}

void displaySetWeatherSummary(const String& line1, const String& line2) {
  weatherLine1 = line1.length() > 0 ? line1 : "Weather unavailable";
  weatherLine2 = line2.length() > 0 ? line2 : "No detail";
  refreshDynamic();
}

void displaySetScheduleSummary(const String& alarmLine, const String& reminderLine, const String& eventLine) {
  if (alarmLine.length() > 0) scheduleLine1 = alarmLine;
  if (reminderLine.length() > 0) scheduleLine2 = reminderLine;
  if (eventLine.length() > 0) scheduleLine3 = eventLine;
  prevCalendarDay = "";
  refreshDynamic();
}

void displayShowMathTool(const String& expression, const String& result, bool showGraph) {
  mathExpr = expression;
  mathResult = result;
  mathVisible = true;
  graphVisible = showGraph;
  currentScreen = ScreenMode::Assistant;
  selectedMenu = 8;
  screenDrawn = false;
  refreshDynamic();
}

void displaySetStatus(StatusState state, const char* msg) {
  currentState = state;
  if (msg && strlen(msg) > 0) {
    currentStatus = msg;
  } else if (state == StatusState::Recording) {
    currentStatus = "Listening";
  } else if (state == StatusState::Sending) {
    currentStatus = "Turning info into bits";
  } else if (state == StatusState::Idle) {
    currentStatus = "Ready";
  } else {
    currentStatus = "Check serial";
  }
  if (state == StatusState::Recording || state == StatusState::Sending) {
    currentScreen = ScreenMode::Assistant;
    selectedMenu = 8;
    screenDrawn = false;
  }
  refreshDynamic();
}

void displayVisualizer(int level, int bars) {
  (void)level;
  currentBars = bars;
  if (currentBars != prevBars && currentScreen == ScreenMode::Assistant) {
    int count = constrain(currentBars / 4, 0, 12);
    tft.fillRect(116, 128, 88, 9, C_BG);
    for (int i = 0; i < 12; i++) {
      uint16_t color = i < count ? C_RED : C_LINE;
      tft.fillRect(118 + i * 7, 135 - ((i % 4) + 4), 4, (i % 4) + 4, color);
    }
    prevBars = currentBars;
  }
  refreshDynamic();
}

void displayRecordingLevel(int level, int bars) {
  displayVisualizer(level, bars);
}

void displayProcessingStep(int step) {
  const char* phrases[6] = {
    "Turning info to bits",
    "Combing the static",
    "Building answers from hope",
    "Borrowing confidence",
    "Polishing syllables",
    "Almost clever"
  };
  step = constrain(step, 0, 5);
  currentState = StatusState::Sending;
  currentStatus = phrases[step];
  currentScreen = ScreenMode::Assistant;
  selectedMenu = 8;
  refreshDynamic();
}

void displayTypewriter(const String& text) {
  lastOutput = text.length() > 0 ? text : "No output.";
  mathVisible = false;
  graphVisible = false;
  currentScreen = ScreenMode::Assistant;
  selectedMenu = 8;
  currentState = StatusState::Idle;
  currentStatus = "Reply ready";
  typewriterChars = 0;
  lastTranscriptAnimMs = millis();
  screenDrawn = false;
  refreshDynamic();
}

void displayShowOutput(const String& text) {
  lastOutput = text.length() > 0 ? text : "No output.";
  mathVisible = false;
  graphVisible = false;
  currentState = StatusState::Idle;
  currentStatus = "Output ready";
  currentScreen = ScreenMode::Assistant;
  selectedMenu = 8;
  typewriterChars = 0;
  lastTranscriptAnimMs = millis();
  refreshDynamic();
}

void displayScrollText(const String& text, int scrollSpeedMs) {
  lastOutput = text.length() > 0 ? text : "No output.";
  currentScreen = ScreenMode::Assistant;
  selectedMenu = 8;
  typewriterChars = lastOutput.length();
  screenDrawn = false;
  refreshDynamic();

  int maxChars = 272 / 6;
  String lines[32];
  int lineCount = wrapText(lastOutput, lines, 32, maxChars);
  int visibleLines = 50 / 13;
  int maxStart = max(0, lineCount - visibleLines);
  for (int start = 0; start <= maxStart; start++) {
    renderWrappedLines(24, 148, 272, 60, lastOutput, lastOutput.length(), start, 1);
    delay(max(50, scrollSpeedMs));
  }
}

void displayShowTranscription(const String& text) {
  lastTranscription = text.length() > 0 ? text : "No transcription.";
  currentState = StatusState::Sending;
  currentStatus = "Turning speech into text";
  currentScreen = ScreenMode::Assistant;
  selectedMenu = 8;
  transcriptionChars = 0;
  lastTranscriptAnimMs = millis();
  refreshDynamic();
}

void displayJoystickLeftRight(int delta) {
  displayNavigateMenu(delta);
}

void display7SegInit() {}

void display7SegClear() {
  timerRemaining = 0;
  timerTotal = 0;
  refreshDynamic();
}

void displayOledShowTimer(int totalSeconds) {
  int next = max(0, totalSeconds);
  bool newTimer = next > timerRemaining || timerTotal <= 0;
  if (newTimer) timerTotal = next;
  timerRemaining = next;
  if (timerRemaining == 0) timerTotal = 0;
  if (timerRemaining > 0 && newTimer) {
    currentScreen = ScreenMode::Timer;
    selectedMenu = 1;
  }
  refreshDynamic();
}

void display7SegShowClock(int hours, int minutes) {
  (void)hours;
  (void)minutes;
  if (currentScreen == ScreenMode::Home) refreshDynamic();
}
