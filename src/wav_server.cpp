#include "wav_server.h"
#include "config.h"
#include <WebServer.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "audio_processor.h"
#include "display_manager.h"

static WebServer server(HTTP_SERVER_PORT);
static int16_t* audioBuffer = nullptr;
static int audioSampleCount = 0;
static bool serverStarted = false;
static StartRecordCallback g_startCallback = nullptr;
static StopRecordCallback g_stopCallback = nullptr;
static IsRecordingCallback g_isRecordingCallback = nullptr;
static GetMetaJsonCallback g_metaJsonCallback = nullptr;
static ProcessCommandCallback g_processCommandCallback = nullptr;
static SetTimerCallback g_setTimerCallback = nullptr;
static String lastTranscription = "No transcription yet";
static String lastOutput = "No output yet";

struct WAVHeader {
  char riff[4] = {'R', 'I', 'F', 'F'};
  uint32_t fileSize;
  char wave[4] = {'W', 'A', 'V', 'E'};
  char fmt[4] = {'f', 'm', 't', ' '};
  uint32_t fmtSize = 16;
  uint16_t format = 1; // PCM
  uint16_t channels = 1;
  uint32_t sampleRate;
  uint32_t byteRate;
  uint16_t blockAlign = 2;
  uint16_t bitsPerSample = 16;
  char data[4] = {'d', 'a', 't', 'a'};
  uint32_t dataSize;
};

void handleWavRequest() {
  Serial.printf("audio.wav request, buffer=%p samples=%d\n", audioBuffer, audioSampleCount);
  if (!audioBuffer || audioSampleCount == 0) {
    server.send(404, "text/plain", "No audio recorded");
    return;
  }

  WAVHeader header;
  header.sampleRate = SAMPLE_RATE;
  header.byteRate = SAMPLE_RATE * 2;
  header.dataSize = audioSampleCount * 2;
  header.fileSize = 36 + header.dataSize;

  server.sendHeader("Content-Length", String(sizeof(header) + header.dataSize));
  server.sendHeader("Content-Type", "audio/wav");
  server.sendHeader("Content-Disposition", "inline; filename=audio.wav");
  server.sendHeader("Cache-Control", "no-cache");
  server.send(200, "audio/wav", "");

  // Send WAV header and audio data directly on the client socket.
  server.client().write((uint8_t*)&header, sizeof(header));
  server.client().write((uint8_t*)audioBuffer, header.dataSize);
  server.client().flush();
  server.client().stop();

  Serial.printf("WAV served: %d samples, %d bytes\n", audioSampleCount, header.dataSize);
}

void handleRoot() {
  String espHost = WiFi.localIP().toString();
  if (espHost == "0.0.0.0" || espHost.length() == 0) {
    espHost = "not connected";
  }
  String html = "<html><head><title>ESP32 Audio Recorder</title><style>body{font-family:Arial; margin:22px;} button{padding:8px 14px;margin:4px;} .meta{background:#f1f1f1;padding:8px; margin:6px 0;border-radius:4px;} .level{font-family:monospace; margin:6px 0;} </style></head><body>";
  html += "<h1>ESP32 Recorder</h1>";
  html += "<div><button onclick='startRec()'>Start</button><button onclick='stopRec()'>Stop</button><button onclick='loadAudio()'>Load Audio</button><button onclick='transcribeAudio()'>Transcribe</button></div>";
  html += String("<p>ESP32 Host: <span id='hostInfo'>") + espHost + "</span></p>";
  html += "<p>Whisper: <span id='whisperStatus'>checking...</span></p>";
  html += "<p>Whisper URL: <input id='whisperUrl' style='width:320px;' value='" + String(WHISPER_SERVER_URL) + "'></p>";
  html += "<p>Status: <span id='status'>offline</span></p>";
  html += "<p>Level: <span id='level'>0</span></p>";
  html += "<audio id='player' controls style='width:100%;'></audio><br>";
  html += "<h3>Recordings</h3><div id='meta'></div>";
  html += "<h3>Transcription</h3><div id='transcription'></div>";
  html += "<h3>AI Output</h3><div id='output'>No output yet</div>";
  html += "<script>";
  html += "const ESP_HOST = window.location.hostname;";
  html += String("const WHISPER_SERVER_FROM_CONFIG='") + WHISPER_SERVER_URL + "';";
  html += "function getWhisperServer(){ const manual = document.getElementById('whisperUrl').value.trim(); if(manual && manual.length>0) return manual; if(WHISPER_SERVER_FROM_CONFIG && WHISPER_SERVER_FROM_CONFIG.length>0) return WHISPER_SERVER_FROM_CONFIG; return window.location.protocol + '//' + window.location.hostname + ':8080/transcribe'; }";
  html += "let prevRecording=false;";
  html += "const ESP_BASE=window.location.origin;";
  html += "async function checkWhisperServer(){ try { const r = await fetch(getWhisperServer(), {method:'OPTIONS'}); if (r.ok || r.status===405) { document.getElementById('whisperStatus').innerText = 'connected'; return true; } } catch(e){ } document.getElementById('whisperStatus').innerText = 'unreachable'; return false; }";
  html += "async function startRec(){ try { await fetch(ESP_BASE + '/api/start'); await updateStatus(); } catch(e){ console.error('startRec error', e); } }";
  html += "async function stopRec(){ try { await fetch(ESP_BASE + '/api/stop'); await updateStatus(); loadAudio(); transcribeAudio(); } catch(e){ console.error('stopRec error', e); } }";
  html += "function loadAudio(){ const a=document.getElementById('player'); a.src=ESP_BASE + '/audio.wav?t=' + Date.now(); a.load(); }";
  html += "async function transcribeAudio(){";
  html += "  document.getElementById('transcription').innerText = 'Transcribing...';";
  html += "  const whisperServer = getWhisperServer(); if (!await checkWhisperServer()) { document.getElementById('transcription').innerText = 'Error: Whisper server unreachable (' + whisperServer + ')'; return; }";
  html += "  try {";
  html += "    const response = await fetch(ESP_BASE + '/audio.wav');";
  html += "    if (!response.ok) throw new Error('audio.wav not available: ' + response.status);";
  html += "    const blob = await response.blob();";
  html += "    const formData = new FormData();";
  html += "    formData.append('file', blob, 'audio.wav');";
  html += "    const transcribeResponse = await fetch(getWhisperServer(), { method: 'POST', body: formData });";
  html += "    if (!transcribeResponse.ok) throw new Error('Whisper transcribe failed: ' + transcribeResponse.status);";
  html += "    const result = await transcribeResponse.json();";
  html += "    document.getElementById('transcription').innerText = result.text;";
  html += "    await fetch(ESP_BASE + '/set-transcription', { method: 'POST', headers: {'Content-Type': 'text/plain'}, body: result.text });";
  html += "    await fetch(ESP_BASE + '/output', { method: 'POST', headers: {'Content-Type': 'text/plain'}, body: result.text });";
  html += "    displayScrolling(result.text);";
  html += "  } catch (error) {";
  html += "    console.error('transcribeAudio error', error);";
  html += "    document.getElementById('transcription').innerText = 'Error: ' + error.message;";
  html += "  }";
  html += "}";
  html += "async function updateOutput(){ try { const r = await fetch(ESP_BASE + '/output'); if(r.ok){ const t = await r.text(); document.getElementById('output').innerText = t; } } catch(e){ console.warn('updateOutput error', e); } }";
  html += "async function updateStatus(){ try { const r = await fetch(ESP_BASE + '/api/status'); if (!r.ok) throw new Error('status fetch failed: ' + r.status); const j = await r.json(); document.getElementById('hostInfo').innerText = ESP_HOST + ':' + window.location.port; const nowRecording = j.recording; if (prevRecording && !nowRecording) { loadAudio(); transcribeAudio(); } prevRecording = nowRecording; document.getElementById('status').innerText = nowRecording ? 'recording' : 'idle'; document.getElementById('level').innerText = j.level; document.getElementById('meta').innerHTML = j.meta; await checkWhisperServer(); } catch(e) { console.warn('updateStatus error', e); document.getElementById('status').innerText = 'offline'; document.getElementById('hostInfo').innerText = 'not connected to ESP32'; document.getElementById('whisperStatus').innerText = 'unknown'; } }";
  html += "async function displayScrolling(text){ await fetch('/scroll', { method: 'POST', body: text }); }";
  html += "setInterval(updateStatus, 500); updateStatus();";
  html += "setInterval(updateOutput, 1000); updateOutput();";
  html += "</script>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void parseTimerCommand(const String& text) {
  if (!g_setTimerCallback) return;

  String lower = text;
  lower.toLowerCase();

  if (lower.indexOf("timer") < 0) return;

  int durationSeconds = 0;
  int idx = lower.indexOf("for ");
  if (idx >= 0) {
    idx += 4;
    // parse simple numeric duration (e.g. 5 or 10)
    String num;
    while (idx < lower.length() && isDigit(lower[idx])) {
      num += lower[idx];
      idx++;
    }
    if (num.length() > 0) {
      int value = num.toInt();
      if (lower.indexOf("hour", idx) >= 0 || lower.indexOf("hr", idx) >= 0) {
        durationSeconds = value * 3600;
      } else if (lower.indexOf("min", idx) >= 0) {
        durationSeconds = value * 60;
      } else if (lower.indexOf("sec", idx) >= 0) {
        durationSeconds = value;
      } else {
        durationSeconds = value * 60;
      }
    }
  }

  if (durationSeconds <= 0) {
    if (lower.indexOf("start a timer") >= 0 || lower.indexOf("start timer") >= 0) {
      durationSeconds = 60;
    }
    if (lower.indexOf("set timer") >= 0 && durationSeconds <= 0) {
      durationSeconds = 60;
    }
  }

  if (durationSeconds > 0) {
    g_setTimerCallback(durationSeconds);
  }
}

void handleStatus() {
  String meta = "";
  if (g_metaJsonCallback) {
    meta = g_metaJsonCallback();
  }
  bool recording = g_isRecordingCallback ? g_isRecordingCallback() : false;
  int level = 0;
  if (recording) {
    level = 1;
  }
  String json = "{\"recording\":" + String(recording ? "true" : "false") + ",\"level\":" + String(level) + ",\"meta\":\"" + meta + "\"}";
  server.send(200, "application/json", json);
}

static String escapeJsonString(const String& value) {
  String result;
  result.reserve(value.length() + 16);
  for (size_t i = 0; i < value.length(); i++) {
    char c = value[i];
    switch (c) {
      case '"': result += "\\\""; break;
      case '\\': result += "\\\\"; break;
      case '\n': result += "\\n"; break;
      case '\r': result += "\\r"; break;
      case '\t': result += "\\t"; break;
      default: result += c; break;
    }
  }
  return result;
}

static bool buildMultipartWav(int16_t* buffer, int sampleCount, uint8_t*& payload, size_t& payloadSize, String& boundary) {
  if (!buffer || sampleCount <= 0) {
    return false;
  }

  WAVHeader header;
  header.sampleRate = SAMPLE_RATE;
  header.byteRate = SAMPLE_RATE * 2;
  header.dataSize = sampleCount * 2;
  header.fileSize = 36 + header.dataSize;

  boundary = "ESP32Boundary123456";
  String prefix = "--" + boundary + "\r\n";
  prefix += "Content-Disposition: form-data; name=\"file\"; filename=\"audio.wav\"\r\n";
  prefix += "Content-Type: audio/wav\r\n\r\n";
  String suffix = "\r\n--" + boundary + "--\r\n";

  size_t prefixLen = prefix.length();
  size_t suffixLen = suffix.length();
  size_t audioBytes = header.dataSize;
  payloadSize = prefixLen + sizeof(header) + audioBytes + suffixLen;
  payload = (uint8_t*)malloc(payloadSize);
  if (!payload) {
    return false;
  }

  memcpy(payload, prefix.c_str(), prefixLen);
  memcpy(payload + prefixLen, &header, sizeof(header));
  memcpy(payload + prefixLen + sizeof(header), buffer, audioBytes);
  memcpy(payload + prefixLen + sizeof(header) + audioBytes, suffix.c_str(), suffixLen);
  return true;
}

static String parseWhisperText(const String& response) {
  int textIndex = response.indexOf("\"text\"");
  if (textIndex < 0) {
    return String();
  }
  int colon = response.indexOf(':', textIndex);
  if (colon < 0) {
    return String();
  }
  int startQuote = response.indexOf('"', colon + 1);
  if (startQuote < 0) {
    return String();
  }
  int endQuote = response.indexOf('"', startQuote + 1);
  if (endQuote < 0) {
    return String();
  }
  return response.substring(startQuote + 1, endQuote);
}

static bool transcribeBufferToWhisper(int16_t* buffer, int sampleCount, String& transcription, String& error) {
  if (!buffer || sampleCount <= 0) {
    error = "No recorded audio available";
    return false;
  }
  if (!WiFi.isConnected()) {
    error = "Wi-Fi disconnected";
    return false;
  }
  if (strlen(WHISPER_SERVER_URL) == 0) {
    error = "WHISPER_SERVER_URL not configured";
    return false;
  }

  uint8_t* postData = nullptr;
  size_t postDataLen = 0;
  String boundary;
  if (!buildMultipartWav(buffer, sampleCount, postData, postDataLen, boundary)) {
    error = "Failed to build audio payload";
    return false;
  }

  HTTPClient http;
  WiFiClient client;
  bool success = false;
  String url = String(WHISPER_SERVER_URL);
  if (!http.begin(client, url)) {
    error = "Whisper begin failed";
  } else {
    http.setTimeout(30000);
    http.addHeader("Content-Type", "multipart/form-data; boundary=" + boundary);
    int httpCode = http.sendRequest("POST", postData, postDataLen);
    if (httpCode > 0) {
      String resp = http.getString();
      if (httpCode >= 200 && httpCode < 300) {
        String text = parseWhisperText(resp);
        if (text.length() > 0) {
          transcription = text;
          success = true;
        } else {
          error = "Invalid Whisper response";
          Serial.printf("Whisper invalid response: %s\n", resp.c_str());
        }
      } else {
        error = "Whisper HTTP " + String(httpCode);
        Serial.printf("Whisper error %d: %s\n", httpCode, resp.c_str());
      }
    } else {
      error = "Whisper send failed";
      Serial.printf("Whisper sendRequest error: %d\n", httpCode);
    }
    http.end();
  }

  free(postData);
  return success;
}

void wavServerAutoTranscribe() {
  if (!audioBuffer || audioSampleCount <= 0) {
    Serial.println("Auto-transcribe skipped: no audio buffer");
    displaySetStatus(StatusState::Error, "No audio recorded");
    return;
  }
  if (!WiFi.isConnected()) {
    Serial.println("Auto-transcribe skipped: Wi-Fi down");
    displaySetStatus(StatusState::Error, "Wi-Fi disconnected");
    return;
  }

  displaySetStatus(StatusState::Sending, "Transcribing...");
  String transcription;
  String error;

  if (!transcribeBufferToWhisper(audioBuffer, audioSampleCount, transcription, error)) {
    Serial.printf("Transcription failed: %s\n", error.c_str());
    displaySetStatus(StatusState::Error, error.c_str());
    return;
  }

  lastTranscription = transcription;
  displayShowTranscription(lastTranscription);
  Serial.printf("Transcription result: %s\n", lastTranscription.c_str());
  displaySetStatus(StatusState::Idle, "Transcribed");
}

void handleTimerStart() {
  if (g_setTimerCallback) {
    int seconds = 60;
    if (server.hasArg("seconds")) {
      seconds = server.arg("seconds").toInt();
      if (seconds <= 0) seconds = 60;
    }
    g_setTimerCallback(seconds);
    server.send(200, "application/json", "{\"status\":\"timer_started\",\"seconds\":" + String(seconds) + "}");
  } else {
    server.send(500, "application/json", "{\"error\":\"no_timer_callback\"}");
  }
}

void handleTimerStop() {
  if (g_setTimerCallback) {
    g_setTimerCallback(0);
    server.send(200, "application/json", "{\"status\":\"timer_stopped\"}");
  } else {
    server.send(500, "application/json", "{\"error\":\"no_timer_callback\"}");
  }
}

void handleTranscription() {
  server.send(200, "text/plain", lastTranscription);
}

void handleTranscribeProxy() {
  if (strlen(WHISPER_SERVER_URL) == 0) {
    server.send(400, "application/json", "{\"error\":\"WHISPER_SERVER_URL not configured\"}");
    return;
  }
  server.send(501, "application/json", "{\"error\":\"Local proxy not implemented\"}");
}

void handleOutputGet() {
  server.send(200, "text/plain", lastOutput);
}

void handleScroll() {
  if (server.hasArg("plain")) {
    String text = server.arg("plain");
    displayScrollText(text, 350);
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "No body");
  }
}

void handleOutputPost() {
  String data;
  if (server.hasArg("plain")) {
    data = server.arg("plain");
  } else if (server.args() > 0) {
    data = server.arg(0);
  } else {
    data = server.arg("body");
  }

  if (data.length() > 0) {
    lastOutput = data;
    for (int step = 0; step < 5; step++) {
      displayProcessingStep(step);
      delay(300);
    }

    if (lastOutput.length() > 80) {
      displayScrollText(lastOutput, 300);
    } else {
      displayTypewriter(lastOutput);
    }

    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "No body");
  }
}

void handleSetTranscription() {
  String text;
  if (server.hasArg("plain")) {
    text = server.arg("plain");
  } else if (server.args() > 0) {
    text = server.arg(0);
  } else {
    text = server.arg("body");
  }

  if (text.length() > 0) {
    lastTranscription = text;
    displayShowTranscription(lastTranscription);
    if (g_processCommandCallback) {
      lastOutput = g_processCommandCallback(lastTranscription);
    }
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "No body");
  }
}

void handleStart() {
  if (g_startCallback) g_startCallback();
  server.send(200, "application/json", "{\"status\":\"started\"}");
}

void handleStop() {
  if (g_stopCallback) g_stopCallback();
  server.send(200, "application/json", "{\"status\":\"stopped\"}");
}

void wavServerInit() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WAV server: Wi-Fi not connected");
    return;
  }

  server.on("/", HTTP_GET, handleRoot);
  server.on("/audio.wav", HTTP_GET, handleWavRequest);
  server.on("/transcription", HTTP_GET, handleTranscription);
  server.on("/output", HTTP_GET, handleOutputGet);
  server.on("/output", HTTP_POST, handleOutputPost);
  server.on("/set-transcription", HTTP_POST, handleSetTranscription);
  server.on("/api/start", HTTP_GET, handleStart);
  server.on("/api/stop", HTTP_GET, handleStop);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/timer/start", HTTP_GET, handleTimerStart);
  server.on("/api/timer/stop", HTTP_GET, handleTimerStop);
  server.on("/transcribe", HTTP_OPTIONS, [](){ server.send(200); });
  server.on("/transcribe", HTTP_POST, handleTranscribeProxy);
  server.on("/scroll", HTTP_POST, handleScroll);

  server.begin();
  serverStarted = true;

  Serial.printf("WAV server started at http://%s:%d\n", WiFi.localIP().toString().c_str(), HTTP_SERVER_PORT);
  Serial.println("  - Index: http://<ip>:8080/");
  Serial.println("  - Raw WAV: http://<ip>:8080/audio.wav");
}

bool wavServerIsRunning() {
  return serverStarted;
}

void wavServerStop() {
  server.stop();
  serverStarted = false;
}

void wavServerHandleClients() {
  if (serverStarted) {
    server.handleClient();
  }
}

void wavServerSetAudioBuffer(int16_t* buffer, int sampleCount) {
  audioBuffer = buffer;
  audioSampleCount = sampleCount;
}

void wavServerSetCallbacks(StartRecordCallback startCb, StopRecordCallback stopCb, IsRecordingCallback isRecordingCb, GetMetaJsonCallback metaCb, ProcessCommandCallback procCmdCb, SetTimerCallback timerCb) {
  g_startCallback = startCb;
  g_stopCallback = stopCb;
  g_isRecordingCallback = isRecordingCb;
  g_metaJsonCallback = metaCb;
  g_processCommandCallback = procCmdCb;
  g_setTimerCallback = timerCb;
}
