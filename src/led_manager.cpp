#include "led_manager.h"
#include "config.h"
#include <FastLED.h>

static CRGB leds[LED_COUNT];
static CRGB ambienceFrame[LED_COUNT];
static LedEffect currentEffect = LedEffect::Rainbow;
static CRGB solidColor = CRGB(255, 96, 24);
static CRGB ambienceColor = CRGB(24, 64, 255);
static uint8_t brightness = LED_BRIGHTNESS;
static uint8_t rainbowHue = 0;
static unsigned long lastFrameMs = 0;
static unsigned long lastAmbienceFrameMs = 0;
static bool hasAmbienceFrame = false;

const char* ledEffectName(LedEffect effect) {
  switch (effect) {
    case LedEffect::Rainbow: return "rainbow";
    case LedEffect::Solid: return "solid";
    case LedEffect::Ambience: return "ambience";
    case LedEffect::Fire: return "fire";
    case LedEffect::Forest: return "forest";
    case LedEffect::Ocean: return "ocean";
  }
  return "rainbow";
}

bool ledParseEffect(const String& value, LedEffect& effect) {
  String name = value;
  name.toLowerCase();
  if (name == "rainbow") effect = LedEffect::Rainbow;
  else if (name == "solid") effect = LedEffect::Solid;
  else if (name == "ambience" || name == "screen") effect = LedEffect::Ambience;
  else if (name == "fire") effect = LedEffect::Fire;
  else if (name == "forest") effect = LedEffect::Forest;
  else if (name == "ocean") effect = LedEffect::Ocean;
  else return false;
  return true;
}

void ledInit() {
  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, LED_COUNT);
  FastLED.setBrightness(brightness);
  FastLED.clear(true);
  ledSetEffect(LedEffect::Rainbow);
}

void ledSetEffect(LedEffect effect) {
  currentEffect = effect;
  lastFrameMs = 0;
}

void ledSetSolidColor(uint8_t r, uint8_t g, uint8_t b) {
  solidColor = CRGB(r, g, b);
  ledSetEffect(LedEffect::Solid);
}

void ledSetBrightness(uint8_t value) {
  brightness = value;
  FastLED.setBrightness(brightness);
}

void ledSetAmbienceColor(uint8_t r, uint8_t g, uint8_t b) {
  CRGB color(r, g, b);
  if (color.r || color.g || color.b) {
    ambienceColor = color;
    lastAmbienceFrameMs = millis();
  }
  ledSetEffect(LedEffect::Ambience);
}

void ledSetAmbienceFrame(const uint8_t* rgb, int byteCount) {
  if (!rgb || byteCount < 3) {
    return;
  }

  int pixelCount = byteCount / 3;
  int usablePixels = min(pixelCount, LED_COUNT);
  uint32_t totalR = 0;
  uint32_t totalG = 0;
  uint32_t totalB = 0;
  int litPixels = 0;

  for (int i = 0; i < usablePixels; i++) {
    uint8_t r = rgb[i * 3];
    uint8_t g = rgb[i * 3 + 1];
    uint8_t b = rgb[i * 3 + 2];
    ambienceFrame[i] = CRGB(r, g, b);
    totalR += r;
    totalG += g;
    totalB += b;
    if (r || g || b) {
      litPixels++;
    }
  }

  for (int i = usablePixels; i < LED_COUNT; i++) {
    ambienceFrame[i] = ambienceFrame[i % usablePixels];
  }

  if (litPixels > 0) {
    ambienceColor = CRGB(totalR / usablePixels, totalG / usablePixels, totalB / usablePixels);
    lastAmbienceFrameMs = millis();
    hasAmbienceFrame = true;
    currentEffect = LedEffect::Ambience;
    for (int i = 0; i < LED_COUNT; i++) {
      leds[i] = ambienceFrame[i];
    }
    FastLED.show();
  }
}

LedEffect ledGetEffect() {
  return currentEffect;
}

static void renderRainbow() {
  fill_rainbow(leds, LED_COUNT, rainbowHue, 7);
  rainbowHue++;
}

static void renderSolid() {
  fill_solid(leds, LED_COUNT, solidColor);
}

static void renderAmbience(unsigned long now) {
  if (hasAmbienceFrame && now - lastAmbienceFrameMs < 900) {
    uint8_t breath = beatsin8(12, 224, 255);
    for (int i = 0; i < LED_COUNT; i++) {
      leds[i] = ambienceFrame[i];
      leds[i].nscale8_video(breath);
    }
    return;
  }

  CRGB color = ambienceColor;
  uint8_t wave = beatsin8(10, 42, 180);
  color.nscale8_video(wave);

  for (int i = 0; i < LED_COUNT; i++) {
    uint8_t shimmer = inoise8(i * 28, now / 8);
    leds[i] = color;
    leds[i].nscale8_video(map(shimmer, 0, 255, 120, 255));
  }
}

static void renderFire(unsigned long now) {
  for (int i = 0; i < LED_COUNT; i++) {
    uint8_t heat = inoise8(i * 34, now / 4);
    uint8_t flicker = beatsin8(17 + (i % 5), 24, 95);
    uint8_t hue = map(heat, 0, 255, 0, 42);
    uint8_t val = qadd8(map(heat, 0, 255, 85, 255), flicker);
    leds[i] = CHSV(hue, 255, val);
    if ((i + now / 70) % 11 == 0) {
      leds[i] += CRGB(80, 35, 0);
    }
  }
}

static void renderForest(unsigned long now) {
  for (int i = 0; i < LED_COUNT; i++) {
    uint8_t leaf = inoise8(i * 22, now / 12);
    uint8_t hue = map(leaf, 0, 255, 72, 118);
    uint8_t val = map(leaf, 0, 255, 45, 210);
    leds[i] = CHSV(hue, 210, val);
  }
}

static void renderOcean(unsigned long now) {
  for (int i = 0; i < LED_COUNT; i++) {
    uint8_t waveA = sin8(i * 14 + now / 18);
    uint8_t waveB = inoise8(i * 30, now / 10);
    uint8_t mix = (waveA / 2) + (waveB / 2);
    uint8_t hue = map(mix, 0, 255, 135, 170);
    uint8_t val = map(waveA, 0, 255, 60, 240);
    leds[i] = CHSV(hue, 230, val);
    if (waveA > 232) {
      leds[i] += CRGB(20, 45, 55);
    }
  }
}

void ledUpdate() {
  unsigned long now = millis();
  if (now - lastFrameMs < 24) {
    return;
  }
  lastFrameMs = now;

  switch (currentEffect) {
    case LedEffect::Rainbow: renderRainbow(); break;
    case LedEffect::Solid: renderSolid(); break;
    case LedEffect::Ambience: renderAmbience(now); break;
    case LedEffect::Fire: renderFire(now); break;
    case LedEffect::Forest: renderForest(now); break;
    case LedEffect::Ocean: renderOcean(now); break;
  }

  FastLED.show();
}

String ledGetStatusJson() {
  String json = "{";
  json += "\"effect\":\"" + String(ledEffectName(currentEffect)) + "\"";
  json += ",\"count\":" + String(LED_COUNT);
  json += ",\"brightness\":" + String(brightness);
  json += ",\"ambienceFrame\":" + String(hasAmbienceFrame ? "true" : "false");
  json += ",\"ambienceAgeMs\":";
  json += hasAmbienceFrame ? String(millis() - lastAmbienceFrameMs) : String(-1);
  json += ",\"color\":\"#";
  char color[7];
  snprintf(color, sizeof(color), "%02X%02X%02X", solidColor.r, solidColor.g, solidColor.b);
  json += color;
  json += "\"}";
  return json;
}
