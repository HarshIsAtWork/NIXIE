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
static bool whisperOnlineStatus = false;
static bool whisperReachableStatus = false;
static bool pendingAutoTranscribe = false;
static String whisperServerUrl = String(WHISPER_SERVER_URL);
static String lastWhisperStatus = "unknown";
static String lastWhisperCheck = "unknown";
static StartRecordCallback g_startCallback = nullptr;
static StopRecordCallback g_stopCallback = nullptr;
static IsRecordingCallback g_isRecordingCallback = nullptr;
static GetMetaJsonCallback g_metaJsonCallback = nullptr;
static ProcessCommandCallback g_processCommandCallback = nullptr;
static SetTimerCallback g_setTimerCallback = nullptr;
static String lastTranscription = "No transcription yet";
static String lastOutput = "No output yet";
static String lastWeather = "Clear";
static int audioGeneration = 0;
static int lastAutoTranscribedGeneration = -1;
static String lastProcessedCommandText;
static String lastDisplayedScrollText;

static String escapeJsonString(const String& value);
static String normalizeWhisperUrl(const String& rawUrl);
static String decodeJsonString(const String& value);

static void sendCorsHeaders() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

static void handleCorsOptions() {
  sendCorsHeaders();
  server.send(204);
}

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

static void fillWavHeader(WAVHeader& header, int sampleCount) {
  header.sampleRate = SAMPLE_RATE;
  header.byteRate = SAMPLE_RATE * header.channels * (header.bitsPerSample / 8);
  header.blockAlign = header.channels * (header.bitsPerSample / 8);
  header.dataSize = sampleCount * header.blockAlign;
  header.fileSize = 36 + header.dataSize;
}

void handleWavRequest() {
  sendCorsHeaders();
  Serial.printf("audio.wav request, buffer=%p samples=%d\n", audioBuffer, audioSampleCount);
  if (!audioBuffer || audioSampleCount == 0) {
    server.send(404, "text/plain", "No audio recorded");
    return;
  }

  WAVHeader header;
  fillWavHeader(header, audioSampleCount);

  const size_t totalBytes = sizeof(header) + header.dataSize;
  WiFiClient client = server.client();

  client.print("HTTP/1.1 200 OK\r\n");
  client.print("Content-Type: audio/wav\r\n");
  client.print("Content-Length: ");
  client.print(totalBytes);
  client.print("\r\n");
  client.print("Content-Disposition: inline; filename=\"audio.wav\"\r\n");
  client.print("Cache-Control: no-store, no-cache, must-revalidate, max-age=0\r\n");
  client.print("Pragma: no-cache\r\n");
  client.print("Accept-Ranges: none\r\n");
  client.print("Connection: close\r\n\r\n");

  client.write((const uint8_t*)&header, sizeof(header));
  client.write((const uint8_t*)audioBuffer, header.dataSize);
  client.flush();
  delay(1);
  client.stop();

  Serial.printf("WAV served: %d samples, %u bytes\n", audioSampleCount, (unsigned)header.dataSize);
}

void handleWavHeadRequest() {
  sendCorsHeaders();
  if (!audioBuffer || audioSampleCount == 0) {
    server.send(404, "text/plain", "No audio recorded");
    return;
  }

  WAVHeader header;
  fillWavHeader(header, audioSampleCount);

  server.sendHeader("Content-Length", String(sizeof(header) + header.dataSize));
  server.sendHeader("Content-Disposition", "inline; filename=audio.wav");
  server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Accept-Ranges", "none");
  server.send(200, "audio/wav", "");
}

void handleRoot() {
  sendCorsHeaders();
  String espHost = WiFi.localIP().toString();
  if (espHost == "0.0.0.0" || espHost.length() == 0) {
    espHost = "not connected";
  }

  String html = "<!DOCTYPE html><html lang='en'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1.0'><title>ESP32 Audio Recorder</title><style>body{margin:16px;font-family:Arial,Helvetica,sans-serif;background:#fff;color:#000;}button{margin:4px;padding:10px 14px;}input,textarea{width:100%;padding:8px;margin:4px 0;box-sizing:border-box;}label{display:block;margin-top:12px;font-weight:600;}div.section{margin-bottom:18px;padding-bottom:12px;border-bottom:1px solid #ddd;}audio{width:100%;margin-top:8px;}</style></head><body>";
  html += "<h1>ESP32 Audio Recorder</h1>";
  html += "<div class='section'><p>Use the buttons below or the physical button to record. Release to stop.</p>";
  html += "<button id='startBtn' onclick='startRec()'>Start recording</button>";
  html += "<button id='stopBtn' onclick='stopRec()'>Stop recording</button>";
  html += "<label for='audioPlayer'>Recorded audio</label>";
  html += "<audio id='audioPlayer' controls></audio>";
  html += "</div>";

  html += "<div class='section'><label for='whisperUrlInput'>Whisper URL</label>";
  html += "<input id='whisperUrlInput' type='text' placeholder='http://192.168.x.x:8080/transcribe' value='" + escapeJsonString(whisperServerUrl) + "'>";
  html += "<button onclick='saveWhisperUrl()'>Save URL</button>";
  html += "<button onclick='checkWhisperUrl()'>Check URL</button>";
  html += "<div id='whisperStatusLine' style='margin-top:8px;font-size:14px;color:#333;'>" + escapeJsonString(whisperServerUrl) + "</div>";
  html += "</div>";

  html += "<div class='section'><label>Status</label>";
  html += "<div>Connection: <span id='status'>loading...</span></div>";
  html += "<div>Recording: <span id='recording'>no</span></div>";
  html += "<div>Samples: <span id='samples'>0</span></div>";
  html += "</div>";

  html += "<div class='section'><label>Transcription</label>";
  html += "<textarea id='transcription' rows='4' readonly>No transcription yet.</textarea>";
  html += "<button onclick='manualTranscribe()'>Transcribe now</button>";
  html += "</div>";

  html += "<div class='section'><label>AI output</label>";
  html += "<textarea id='output' rows='4' readonly>No output yet.</textarea>";
  html += "</div>";

  html += "<div class='section'><label>ESP32 host</label>";
  html += "<div>" + espHost + ":8080</div>";
  html += "</div>";

  html += "<script>const ESP_BASE=window.location.origin;const AUDIO_PATH=ESP_BASE+'/audio.wav';let connectionOk=false;let audioLoaded=false;let lastAudioVersion=0;async function testConnection(){try{const res=await fetch(ESP_BASE+'/ping',{method:'GET',cache:'no-cache'});if(res.ok){connectionOk=true;return true;}throw new Error('HTTP '+res.status);}catch(e){connectionOk=false;return false;}}async function startRec(){if(!connectionOk&&!await testConnection()){alert('No connection');return;}await fetch(ESP_BASE+'/api/start',{method:'GET'});await updateStatus();}async function stopRec(){if(!connectionOk&&!await testConnection()){alert('No connection');return;}await fetch(ESP_BASE+'/api/stop',{method:'GET'});await updateStatus();}async function saveWhisperUrl(){if(!connectionOk&&!await testConnection()){alert('No connection');return;}const url=document.getElementById('whisperUrlInput').value.trim();if(!url){alert('Enter a URL');return;}const res=await fetch(ESP_BASE+'/api/whisper',{method:'POST',headers:{'Content-Type':'text/plain'},body:url});if(!res.ok){document.getElementById('whisperStatusLine').innerText='Save failed';return;}const j=await res.json();document.getElementById('whisperStatusLine').innerText=j.whisperStatus||'saved';await updateStatus();}async function checkWhisperUrl(){if(!connectionOk&&!await testConnection()){alert('No connection');return;}const url=document.getElementById('whisperUrlInput').value.trim();if(!url){alert('Enter a URL');return;}const res=await fetch(ESP_BASE+'/api/whisper',{method:'POST',headers:{'Content-Type':'text/plain'},body:url});if(!res.ok){document.getElementById('whisperStatusLine').innerText='Check failed';return;}const j=await res.json();document.getElementById('whisperStatusLine').innerText=j.whisperStatus||'checked';await updateStatus();}async function manualTranscribe(){if(!connectionOk&&!await testConnection()){alert('No connection');return;}const res=await fetch(ESP_BASE+'/transcribe',{method:'POST'});if(!res.ok){document.getElementById('whisperStatusLine').innerText='Transcribe failed';return;}const j=await res.json();if(j.transcription){document.getElementById('transcription').value=j.transcription;}await updateStatus();}async function updateStatus(){try{const res=await fetch(ESP_BASE+'/api/status');if(!res.ok)throw new Error('status');const j=await res.json();connectionOk=true;document.getElementById('status').innerText=j.service?'connected':'offline';document.getElementById('recording').innerText=j.recording?'yes':'no';document.getElementById('samples').innerText=j.samples;document.getElementById('transcription').value=j.transcription||'No transcription yet.';document.getElementById('whisperUrlInput').value=j.whisperUrl||'';document.getElementById('whisperStatusLine').innerText=j.whisperStatus||'';document.getElementById('output').value=j.output||'No output yet.';if(j.hasAudio && j.audioVersion && j.audioVersion !== lastAudioVersion){const player=document.getElementById('audioPlayer');player.src=AUDIO_PATH+'?v='+j.audioVersion+'&t='+Date.now();player.load();audioLoaded=true;lastAudioVersion=j.audioVersion;}else if(!j.hasAudio){document.getElementById('audioPlayer').removeAttribute('src');audioLoaded=false;lastAudioVersion=0;} }catch(e){connectionOk=false;document.getElementById('status').innerText='offline';}}setInterval(updateStatus,500);updateStatus();</script>";
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
  sendCorsHeaders();
  String meta = "";
  if (g_metaJsonCallback) {
    meta = g_metaJsonCallback();
  }
  bool recording = g_isRecordingCallback ? g_isRecordingCallback() : false;
  int level = 0;
  if (recording) {
    level = 1;
  }
  String json = "{\"recording\":" + String(recording ? "true" : "false") + ",\"level\":" + String(level) + ",\"samples\":" + String(audioSampleCount) + ",\"audioVersion\":" + String(audioGeneration) + ",\"hasAudio\":" + String(audioSampleCount > 0 ? "true" : "false") + ",\"service\":" + String(serverStarted ? "true" : "false") + ",\"whisper\":" + String(whisperOnlineStatus ? "true" : "false") + ",\"whisperReachable\":" + String(whisperReachableStatus ? "true" : "false") + ",\"whisperUrl\":\"" + escapeJsonString(whisperServerUrl) + "\",\"whisperStatus\":\"" + escapeJsonString(lastWhisperStatus) + "\",\"transcription\":\"" + escapeJsonString(lastTranscription) + "\",\"output\":\"" + escapeJsonString(lastOutput) + "\",\"meta\":\"" + escapeJsonString(meta) + "\"}";
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

static String normalizeWhisperUrl(const String& rawUrl) {
  String url = rawUrl;
  url.trim();

  if (url.length() == 0) {
    return url;
  }

  if (!url.startsWith("http://") && !url.startsWith("https://")) {
    url = "http://" + url;
  }

  int schemeIdx = url.indexOf("://");
  int pathIdx = url.indexOf('/', schemeIdx >= 0 ? schemeIdx + 3 : 0);
  if (pathIdx < 0) {
    url += "/transcribe";
    return url;
  }

  String path = url.substring(pathIdx);
  if (path == "/") {
    url.remove(pathIdx);
    url += "/transcribe";
  } else if (!path.startsWith("/transcribe")) {
    if (url.endsWith("/")) {
      url += "transcribe";
    } else {
      url += "/transcribe";
    }
  }

  return url;
}

static String decodeJsonString(const String& value) {
  String result;
  result.reserve(value.length());

  bool escaping = false;
  for (size_t i = 0; i < value.length(); i++) {
    char c = value[i];
    if (!escaping) {
      if (c == '\\') {
        escaping = true;
      } else {
        result += c;
      }
      continue;
    }

    switch (c) {
      case '"': result += '"'; break;
      case '\\': result += '\\'; break;
      case '/': result += '/'; break;
      case 'b': result += '\b'; break;
      case 'f': result += '\f'; break;
      case 'n': result += '\n'; break;
      case 'r': result += '\r'; break;
      case 't': result += '\t'; break;
      default: result += c; break;
    }
    escaping = false;
  }

  if (escaping) {
    result += '\\';
  }

  result.trim();
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

static String getRequestBody() {
  if (server.hasArg("plain")) {
    return server.arg("plain");
  }
  if (server.args() > 0) {
    return server.arg(0);
  }
  return server.arg("body");
}

static bool probeWhisperServer(String& status, bool& reachable) {
  reachable = false;
  String url = normalizeWhisperUrl(whisperServerUrl);
  if (url.length() == 0) {
    status = "No URL configured";
    return false;
  }

  HTTPClient http;
  WiFiClient client;
  if (!http.begin(client, url)) {
    status = "Invalid URL";
    return false;
  }

  static const char* probeBody = "--ESP32Probe\r\nContent-Disposition: form-data; name=\"file\"; filename=\"probe.wav\"\r\nContent-Type: audio/wav\r\n\r\n\r\n--ESP32Probe--\r\n";
  http.setTimeout(4000);
  http.addHeader("Content-Type", "multipart/form-data; boundary=ESP32Probe");
  int httpCode = http.sendRequest("POST", (uint8_t*)probeBody, strlen(probeBody));
  http.end();

  if (httpCode > 0) {
    if (httpCode == 422 || httpCode == 400 || httpCode == 405 || httpCode == 415 || (httpCode >= 200 && httpCode < 300)) {
      reachable = true;
      status = "connected";
      return true;
    }
    if (httpCode == 404) {
      status = "wrong path";
      return false;
    }
    status = "HTTP " + String(httpCode);
    return false;
  }

  status = "connect failed";
  return false;
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

  bool escaping = false;
  for (int i = startQuote + 1; i < response.length(); i++) {
    char c = response[i];
    if (escaping) {
      escaping = false;
      continue;
    }
    if (c == '\\') {
      escaping = true;
      continue;
    }
    if (c == '"') {
      return decodeJsonString(response.substring(startQuote + 1, i));
    }
  }

  return String();
}

static bool transcribeBufferToWhisper(int16_t* buffer, int sampleCount, String& transcription, String& error) {
  whisperOnlineStatus = false;
  lastWhisperStatus = "checking";
  if (!buffer || sampleCount <= 0) {
    error = "No recorded audio available";
    lastWhisperStatus = error;
    return false;
  }
  if (!WiFi.isConnected()) {
    error = "Wi-Fi disconnected";
    lastWhisperStatus = error;
    return false;
  }
  if (whisperServerUrl.length() == 0) {
    error = "WHISPER_SERVER_URL not configured";
    lastWhisperStatus = error;
    return false;
  }
  whisperServerUrl = normalizeWhisperUrl(whisperServerUrl);

  uint8_t* postData = nullptr;
  size_t postDataLen = 0;
  String boundary;
  if (!buildMultipartWav(buffer, sampleCount, postData, postDataLen, boundary)) {
    error = "Failed to build audio payload";
    lastWhisperStatus = error;
    return false;
  }

  HTTPClient http;
  WiFiClient client;
  bool success = false;
  String url = whisperServerUrl;
  if (!http.begin(client, url)) {
    error = "Whisper begin failed";
    lastWhisperStatus = error;
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
          whisperOnlineStatus = true;
          whisperReachableStatus = true;
          lastWhisperCheck = "connected";
          lastWhisperStatus = "connected";
        } else {
          error = "Invalid Whisper response";
          lastWhisperStatus = error;
          Serial.printf("Whisper invalid response: %s\n", resp.c_str());
        }
      } else {
        error = "Whisper HTTP " + String(httpCode);
        lastWhisperStatus = error;
        Serial.printf("Whisper error %d: %s\n", httpCode, resp.c_str());
      }
    } else {
      error = "Whisper send failed";
      lastWhisperStatus = error;
      Serial.printf("Whisper sendRequest error: %d\n", httpCode);
    }
    http.end();
  }

  free(postData);
  if (!success) {
    whisperReachableStatus = false;
  }
  return success;
}

void wavServerAutoTranscribe() {
  if (!audioBuffer || audioSampleCount <= 0) {
    Serial.println("Auto-transcribe skipped: no audio buffer");
    displaySetStatus(StatusState::Error, "No audio recorded");
    displaySetServiceStatus(serverStarted, false);
    whisperOnlineStatus = false;
    return;
  }
  if (audioGeneration == lastAutoTranscribedGeneration) {
    Serial.println("Auto-transcribe skipped: recording already processed");
    displaySetStatus(StatusState::Idle, "Waiting for next press");
    return;
  }
  if (!WiFi.isConnected()) {
    Serial.println("Auto-transcribe skipped: Wi-Fi down");
    displaySetStatus(StatusState::Error, "Wi-Fi disconnected");
    displaySetServiceStatus(serverStarted, false);
    whisperOnlineStatus = false;
    return;
  }

  displaySetStatus(StatusState::Sending, "Sending audio to servers");
  displaySetServiceStatus(serverStarted, false);
  String transcription;
  String error;

  if (!transcribeBufferToWhisper(audioBuffer, audioSampleCount, transcription, error)) {
    Serial.printf("Transcription failed: %s\n", error.c_str());
    displaySetStatus(StatusState::Error, "STT failed");
    displaySetServiceStatus(serverStarted, whisperOnlineStatus);
    return;
  }

  lastAutoTranscribedGeneration = audioGeneration;
  lastTranscription = transcription;
  displayShowTranscription(lastTranscription);
  if (g_processCommandCallback) {
    String offlineResponse = g_processCommandCallback(lastTranscription);
    if (offlineResponse.length() > 0) {
      lastOutput = offlineResponse;
    }
  }
  Serial.printf("Transcription result: %s\n", lastTranscription.c_str());
  displaySetStatus(StatusState::Idle, "Transcribed");
  displaySetServiceStatus(serverStarted, whisperOnlineStatus);
}

void handleTimerStart() {
  sendCorsHeaders();
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
  sendCorsHeaders();
  if (g_setTimerCallback) {
    g_setTimerCallback(0);
    server.send(200, "application/json", "{\"status\":\"timer_stopped\"}");
  } else {
    server.send(500, "application/json", "{\"error\":\"no_timer_callback\"}");
  }
}

void handleTranscription() {
  sendCorsHeaders();
  server.send(200, "text/plain", lastTranscription);
}

void handleTranscribeInfo() {
  sendCorsHeaders();
  server.send(200, "text/plain", "Use POST /transcribe to send recorded audio to Whisper and receive transcription.");
}

void handleTranscribeProxy() {
  sendCorsHeaders();
  if (whisperServerUrl.length() == 0) {
    server.send(400, "application/json", "{\"error\":\"WHISPER_SERVER_URL not configured\"}");
    return;
  }
  if (!audioBuffer || audioSampleCount == 0) {
    server.send(400, "application/json", "{\"error\":\"No audio recorded\"}");
    return;
  }
  String transcription;
  String error;
  if (!transcribeBufferToWhisper(audioBuffer, audioSampleCount, transcription, error)) {
    server.send(500, "application/json", "{\"error\":\"" + escapeJsonString(error) + "\"}");
    return;
  }
  lastTranscription = transcription;
  displayShowTranscription(lastTranscription);
  if (g_processCommandCallback) {
    String offlineResponse = g_processCommandCallback(lastTranscription);
    if (offlineResponse.length() > 0) {
      lastOutput = offlineResponse;
    }
  }
  server.send(200, "application/json", "{\"transcription\":\"" + escapeJsonString(transcription) + "\"}");
}

void handlePing() {
  sendCorsHeaders();
  server.send(200, "application/json", "{\"status\":\"ok\",\"timestamp\":" + String(millis()) + "}");
}

void handleFavicon() {
  sendCorsHeaders();
  server.send(204, "image/x-icon", "");
}

void handleWhisperConfigGet() {
  sendCorsHeaders();
  String json = "{\"whisperUrl\":\"" + escapeJsonString(whisperServerUrl) + "\",\"whisperReachable\":" + String(whisperReachableStatus ? "true" : "false") + ",\"whisperStatus\":\"" + escapeJsonString(lastWhisperCheck) + "\"}";
  server.send(200, "application/json", json);
}

void handleWhisperConfigPost() {
  sendCorsHeaders();
  String url = getRequestBody();
  if (url.length() == 0) {
    server.send(400, "application/json", "{\"error\":\"No URL provided\"}");
    return;
  }
  whisperServerUrl = normalizeWhisperUrl(url);
  bool reachable = false;
  String status;
  probeWhisperServer(status, reachable);
  whisperReachableStatus = reachable;
  lastWhisperCheck = status;
  lastWhisperStatus = status;
  String json = "{\"whisperUrl\":\"" + escapeJsonString(whisperServerUrl) + "\",\"whisperReachable\":" + String(reachable ? "true" : "false") + ",\"whisperStatus\":\"" + escapeJsonString(status) + "\"}";
  server.send(200, "application/json", json);
}

void handleOutputGet() {
  sendCorsHeaders();
  server.send(200, "text/plain", lastOutput);
}

void handleScroll() {
  sendCorsHeaders();
  if (server.hasArg("plain")) {
    String text = server.arg("plain");
    if (text == lastDisplayedScrollText) {
      server.send(200, "text/plain", "DUPLICATE");
      return;
    }
    lastDisplayedScrollText = text;
    displayShowOutput(text);
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "No body");
  }
}

void handleOutputPost() {
  sendCorsHeaders();
  String data;
  if (server.hasArg("plain")) {
    data = server.arg("plain");
  } else if (server.args() > 0) {
    data = server.arg(0);
  } else {
    data = server.arg("body");
  }

  if (data.length() > 0) {
    if (data == lastOutput) {
      server.send(200, "text/plain", "DUPLICATE");
      return;
    }
    lastOutput = data;
    displaySetStatus(StatusState::Idle, "Reply ready");
    displayShowOutput(lastOutput);
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "No body");
  }
}

void handleSetTranscription() {
  sendCorsHeaders();
  String text;
  if (server.hasArg("plain")) {
    text = server.arg("plain");
  } else if (server.args() > 0) {
    text = server.arg(0);
  } else {
    text = server.arg("body");
  }

  if (text.length() > 0) {
    if (text == lastProcessedCommandText) {
      server.send(200, "text/plain", "DUPLICATE");
      return;
    }
    lastProcessedCommandText = text;
    lastTranscription = text;
    displayShowTranscription(lastTranscription);
    // Do NOT automatically process transcription as command output
    // Let the user manually trigger command processing via UI
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "No body");
  }
}

void handleStart() {
  sendCorsHeaders();
  if (g_startCallback) g_startCallback();
  server.send(200, "application/json", "{\"status\":\"started\"}");
}

void handleStop() {
  sendCorsHeaders();
  if (g_stopCallback) g_stopCallback();
  server.send(200, "application/json", "{\"status\":\"stopped\"}");
}

void wavServerInit() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WAV server: Wi-Fi not connected");
    return;
  }

  whisperServerUrl = normalizeWhisperUrl(whisperServerUrl);

  server.on("/", HTTP_GET, handleRoot);
  server.on("/ping", HTTP_GET, handlePing);
  server.on("/ping", HTTP_OPTIONS, handleCorsOptions);
  server.on("/audio.wav", HTTP_GET, handleWavRequest);
  server.on("/audio.wav", HTTP_HEAD, handleWavHeadRequest);
  server.on("/transcription", HTTP_GET, handleTranscription);
  server.on("/output", HTTP_GET, handleOutputGet);
  server.on("/output", HTTP_POST, handleOutputPost);
  server.on("/set-transcription", HTTP_POST, handleSetTranscription);
  server.on("/api/start", HTTP_GET, handleStart);
  server.on("/api/start", HTTP_OPTIONS, handleCorsOptions);
  server.on("/api/stop", HTTP_GET, handleStop);
  server.on("/api/stop", HTTP_OPTIONS, handleCorsOptions);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/status", HTTP_OPTIONS, handleCorsOptions);
  server.on("/api/whisper", HTTP_GET, handleWhisperConfigGet);
  server.on("/api/whisper", HTTP_POST, handleWhisperConfigPost);
  server.on("/api/whisper", HTTP_OPTIONS, handleCorsOptions);
  server.on("/api/timer/start", HTTP_GET, handleTimerStart);
  server.on("/api/timer/start", HTTP_OPTIONS, handleCorsOptions);
  server.on("/api/timer/stop", HTTP_GET, handleTimerStop);
  server.on("/api/timer/stop", HTTP_OPTIONS, handleCorsOptions);
  server.on("/transcribe", HTTP_GET, handleTranscribeInfo);
  server.on("/transcribe", HTTP_OPTIONS, handleCorsOptions);
  server.on("/transcribe", HTTP_POST, handleTranscribeProxy);
  server.on("/scroll", HTTP_POST, handleScroll);
  server.on("/scroll", HTTP_OPTIONS, handleCorsOptions);
  server.on("/favicon.ico", HTTP_GET, handleFavicon);
  server.on("/favicon.ico", HTTP_OPTIONS, handleCorsOptions);

  server.begin();
  serverStarted = true;

  if (WiFi.isConnected()) {
    probeWhisperServer(lastWhisperCheck, whisperReachableStatus);
    lastWhisperStatus = lastWhisperCheck;
  }
  displaySetServiceStatus(true, whisperReachableStatus);

  Serial.printf("WAV server started at http://%s:%d\n", WiFi.localIP().toString().c_str(), HTTP_SERVER_PORT);
  Serial.println("  - Index: http://<ip>:8080/");
  Serial.println("  - Raw WAV: http://<ip>:8080/audio.wav");
  Serial.printf("  - Whisper: %s (%s)\n", whisperServerUrl.c_str(), lastWhisperStatus.c_str());
}

bool wavServerIsRunning() {
  return serverStarted;
}

void wavServerStop() {
  server.stop();
  serverStarted = false;
  displaySetServiceStatus(false, false);
}

void wavServerHandleClients() {
  if (serverStarted) {
    server.handleClient();
  }
}

void wavServerSetAudioBuffer(int16_t* buffer, int sampleCount) {
  audioBuffer = buffer;
  audioSampleCount = sampleCount;
  audioGeneration++;
}

void wavServerRequestTranscription() {
  pendingAutoTranscribe = true;
}

void wavServerProcessPending() {
  if (!pendingAutoTranscribe) {
    return;
  }
  pendingAutoTranscribe = false;
  if (audioBuffer && audioSampleCount > 0) {
    wavServerAutoTranscribe();
  }
}

void wavServerSetCallbacks(StartRecordCallback startCb, StopRecordCallback stopCb, IsRecordingCallback isRecordingCb, GetMetaJsonCallback metaCb, ProcessCommandCallback procCmdCb, SetTimerCallback timerCb) {
  g_startCallback = startCb;
  g_stopCallback = stopCb;
  g_isRecordingCallback = isRecordingCb;
  g_metaJsonCallback = metaCb;
  g_processCommandCallback = procCmdCb;
  g_setTimerCallback = timerCb;
}
