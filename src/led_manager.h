#pragma once

#include <Arduino.h>

enum class LedEffect {
  Rainbow,
  Solid,
  Ambience,
  Fire,
  Forest,
  Ocean
};

void ledInit();
void ledUpdate();
void ledSetEffect(LedEffect effect);
void ledSetSolidColor(uint8_t r, uint8_t g, uint8_t b);
void ledSetBrightness(uint8_t brightness);
void ledSetAmbienceFrame(const uint8_t* rgb, int byteCount);
void ledSetAmbienceColor(uint8_t r, uint8_t g, uint8_t b);
LedEffect ledGetEffect();
String ledGetStatusJson();
bool ledParseEffect(const String& value, LedEffect& effect);
const char* ledEffectName(LedEffect effect);
