#pragma once

#include <Arduino.h>

bool audioInit();
bool audioReadAndCompute(float &avgAbs, int &level, int &bars);

void audioStartBuffering();
void audioStopBuffering();
int16_t* audioGetBuffer();
int audioGetBufferSampleCount();
