#pragma once

#include <cstdint>
#include <Arduino.h>

using StartRecordCallback = void (*)();
using StopRecordCallback = void (*)();
using IsRecordingCallback = bool (*)();
using GetMetaJsonCallback = String (*)();
using ProcessCommandCallback = String (*)(const String& transcription);
using SetTimerCallback = void (*)(int totalSeconds);

void wavServerInit();
void wavServerStop();
bool wavServerIsRunning();
void wavServerHandleClients();
void wavServerSetAudioBuffer(int16_t* buffer, int sampleCount);
void wavServerSetCallbacks(StartRecordCallback startCb, StopRecordCallback stopCb, IsRecordingCallback isRecordingCb, GetMetaJsonCallback metaCb, ProcessCommandCallback procCmdCb, SetTimerCallback timerCb);
void wavServerAutoTranscribe();
