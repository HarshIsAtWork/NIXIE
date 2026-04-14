#include "display_manager.h"
#include "config.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
// #include <Adafruit_LEDBackpack.h>

static Adafruit_SSD1306 display(128, 64, &Wire);
static bool display_ok = false;
// static Adafruit_7segment sevenSeg;
// static bool sevenSeg_ok = false;

void displayInit() {
  Wire.begin(OLED_SDA, OLED_SCL);
  display_ok = display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  // sevenSeg_ok = sevenSeg.begin(SEVEN_SEG_I2C_ADDR);
  if (!display_ok) {
    Serial.println("OLED init failed");
    return;
  }
  // if (!sevenSeg_ok) {
  //   Serial.println("7-segment init failed");
  // } else {
  //   Serial.println("7-segment init OK");
  // }
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.clearDisplay();
  display.display();
}

static void showText(const char* line1, const char* line2 = "", int line2y = 32) {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println(line1);
  display.setCursor(0, line2y);
  display.println(line2);
  display.display();
}

void displayBootAnimation() {
  if (!display_ok) return;
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(2);

  for (int i = 0; i < 6; i++) {
    display.clearDisplay();
    display.setCursor(12, 8);
    display.println("NIXIE");

    if (i % 2 == 0) {
      display.setTextSize(1);
      display.setCursor(10, 42);
      display.println("BOOTING...");
    } else {
      display.setTextSize(1);
      display.setCursor(10, 42);
      display.println("starting...");
    }

    display.display();
    delay(350);
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("NIXIE Ready");
  display.display();
  delay(400);
}

void displayVisualizer(int level, int bars) {
  if (!display_ok) return;
  display.clearDisplay();

  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Visualizer");

  int centerY = 30;
  int width = 4;
  int spacing = 2;
  int maxBars = 20;

  int base = map(level, 0, 100, 1, maxBars);

  unsigned long now = millis();
  int shake = (now / 100) % 3;

  for (int i = 0; i < maxBars; i++) {
    int h = map((i + shake) % maxBars, 0, maxBars, 5, 38);
    int x = 2 + i * (width + spacing);
    int y = centerY;
    int h2 = (i < base) ? h : h / 4;

    display.drawFastVLine(x, y - h2, h2, SSD1306_WHITE);
  }

  display.setCursor(0, 52);
  display.setTextSize(1);
  display.print("lvl ");
  display.print(level);
  display.print("  bars ");
  display.print(bars);
  display.display();
}

void displayProcessingStep(int step) {
  if (!display_ok) return;
  const char* phrases[5] = {
    "Processing...",
    "Translating your thoughts",
    "Encoding to binary bits",
    "Analyzing context",
    "Preparing output"
  };

  if (step < 0) step = 0;
  if (step > 4) step = 4;

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(phrases[step]);
  display.display();
}

void displayTypewriter(const String& text) {
  if (!display_ok) return;
  display.clearDisplay();
  display.setTextSize(1);

  const int maxChars = 21;
  const int maxLines = 4;

  String parts[maxLines];
  int idx = 0;
  int start = 0;
  while (start < text.length() && idx < maxLines) {
    int remaining = int(text.length() - start);
    int len = min(maxChars, remaining);
    parts[idx++] = text.substring(start, start + len);
    start += len;
  }

  for (int i = 0; i < idx; i++) {
    for (int p = 0; p <= parts[i].length(); p++) {
      display.clearDisplay();
      display.setCursor(0, 0);
      for (int j = 0; j <= i; j++) {
        display.setCursor(0, j * 13);
        if (j < i) {
          display.print(parts[j]);
        } else {
          display.print(parts[j].substring(0, p));
        }
      }
      display.display();
      delay(40);
    }
  }
}

void displayShowOutput(const String& text) {
  if (!display_ok) return;

  const int maxChars = 21;
  display.clearDisplay();
  display.setTextSize(1);

  int totalLines = (text.length() + maxChars - 1) / maxChars;
  for (int i = 0; i < 4 && i < totalLines; i++) {
    int start = i * maxChars;
    int end = min(start + maxChars, int(text.length()));
    String line = text.substring(start, end);
    display.setCursor(0, i * 13);
    display.println(line);
  }
  display.display();
}

void display7SegClear() {
  // if (!sevenSeg_ok) return;
  // sevenSeg.clear();
  // sevenSeg.writeDisplay();
}

void displayOledShowTimer(int totalSeconds) {
  if (!display_ok) return;
  if (totalSeconds < 0) totalSeconds = 0;

  int hours = totalSeconds / 3600;
  int minutes = (totalSeconds % 3600) / 60;
  int seconds = totalSeconds % 60;

  display.clearDisplay();
  display.setTextSize(3); // Large font
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 20); // Center-ish

  char buf[9];
  snprintf(buf, sizeof(buf), "%02d:%02d:%02d", hours, minutes, seconds);
  display.println(buf);
  display.display();
}

void display7SegShowClock(int hours, int minutes) {
  // if (!sevenSeg_ok) return;
  // if (hours < 0) hours = 0;
  // if (hours > 23) hours = 23;
  // if (minutes < 0) minutes = 0;
  // if (minutes > 59) minutes = 59;

  // int value = (hours / 10) * 1000 + (hours % 10) * 100 + (minutes / 10) * 10 + (minutes % 10);
  // sevenSeg.clear();
  // sevenSeg.drawColon(true);
  // sevenSeg.print(value, DEC);
  // sevenSeg.writeDisplay();
}

void displayScrollText(const String& text, int scrollSpeedMs) {
  if (!display_ok) return;

  const int maxChars = 21;
  const int linesPerPage = 4;
  int totalLines = (text.length() + maxChars - 1) / maxChars;

  const int maxLines = 30;
  String lines[maxLines];
  int lineCount = 0;
  for (int i = 0; i < totalLines && lineCount < maxLines; i++) {
    int start = i * maxChars;
    int end = min(start + maxChars, int(text.length()));
    lines[lineCount++] = text.substring(start, end);
  }

  for (int startLine = 0; startLine <= lineCount - linesPerPage; startLine++) {
    display.clearDisplay();
    display.setTextSize(1);
    for (int r = 0; r < linesPerPage; r++) {
      display.setCursor(0, r * 13);
      display.println(lines[startLine + r]);
    }
    display.display();
    delay(scrollSpeedMs);
  }

  // Show final page if any remaining lines (when totalLines < linesPerPage)
  if (totalLines < linesPerPage) {
    display.clearDisplay();
    display.setTextSize(1);
    for (int r = 0; r < totalLines; r++) {
      display.setCursor(0, r * 13);
      display.println(lines[r]);
    }
    display.display();
  }
}

void displayRecordingLevel(int level, int bars) {
  if (!display_ok) return;

  // show visualizer while button is down used externally in main loop
  displayVisualizer(level, bars);
}

void displaySetStatus(StatusState state, const char* msg) {
  if (!display_ok) {
    return;
  }

  switch (state) {
    case StatusState::Idle:
      showText("Press button to", "start recording");
      break;
    case StatusState::WaitingForButton:
      showText("Waiting for button", "press...");
      break;
    case StatusState::Recording:
      showText("Recording audio", msg ? msg : "...reading...");
      break;
    case StatusState::Sending:
      showText("Sending to n8n", msg ? msg : "...please wait");
      break;
    case StatusState::Error:
      showText("ERROR", msg ? msg : "Check serial");
      break;
  }
}

void displayShowTranscription(const String& text) {
  if (!display_ok) {
    return;
  }

  String full = text;
  const int maxChars = 21;
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Transcription:");
  for (int i = 0; i < 3; i++) {
    int start = i * maxChars;
    if (start >= full.length()) break;
    int end = min(start + maxChars, int(full.length()));
    display.setCursor(0, 14 + i * 12);
    display.println(full.substring(start, end));
  }
  display.display();
}

