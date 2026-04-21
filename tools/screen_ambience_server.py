from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import argparse


PAGE = r"""<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>ESP32 Screen Ambience</title>
  <style>
    * { box-sizing: border-box; }
    body { margin: 0; font-family: Arial, sans-serif; background: #111; color: #f7f7f7; }
    main { max-width: 760px; margin: auto; padding: 22px; }
    label { display: block; margin: 14px 0 6px; }
    input { width: 100%; padding: 10px; border-radius: 8px; border: 1px solid #555; background: #181818; color: #fff; }
    button { margin-top: 14px; margin-right: 8px; padding: 10px 14px; border: 0; border-radius: 8px; background: #24a0ed; color: #fff; cursor: pointer; }
    button.secondary { background: #333; }
    .status { margin-top: 16px; padding: 12px; border: 1px solid #333; border-radius: 8px; background: #181818; min-height: 44px; }
    .row { display: grid; grid-template-columns: 1fr 140px; gap: 12px; align-items: end; }
    @media (max-width: 560px) { .row { grid-template-columns: 1fr; } }
  </style>
</head>
<body>
<main>
  <h1>ESP32 Screen Ambience</h1>
  <p>Share a screen or window, then this localhost page streams sampled colors to the ESP32 LEDs.</p>
  <label for="espBase">ESP32 URL</label>
  <input id="espBase" value="http://ESP32_IP:8080" spellcheck="false">
  <div class="row">
    <div>
      <label for="ledCount">LED count</label>
      <input id="ledCount" type="number" min="1" max="300" value="57">
    </div>
    <button onclick="probeEsp()">Check ESP</button>
  </div>
  <button onclick="startAmbience()">Start screen ambience</button>
  <button class="secondary" onclick="stopAmbience()">Stop</button>
  <div class="status" id="status">Waiting.</div>
</main>
<script>
let timer = null;
let stream = null;

function baseUrl() {
  return document.getElementById('espBase').value.trim().replace(/\/+$/, '');
}

function setStatus(text) {
  document.getElementById('status').innerText = text;
}

async function probeEsp() {
  try {
    const response = await fetch(baseUrl() + '/api/led');
    if (!response.ok) throw new Error('HTTP ' + response.status);
    const data = await response.json();
    if (data.count) document.getElementById('ledCount').value = data.count;
    setStatus('Connected. Effect: ' + data.effect + ', LEDs: ' + data.count);
  } catch (error) {
    setStatus('Could not reach ESP32 LED API: ' + error.message);
  }
}

async function startAmbience() {
  stopAmbience();
  const esp = baseUrl();
  const ledCount = Math.max(1, Math.min(300, parseInt(document.getElementById('ledCount').value || '57', 10)));

  try {
    stream = await navigator.mediaDevices.getDisplayMedia({ video: true, audio: false });
    const video = document.createElement('video');
    video.srcObject = stream;
    video.muted = true;
    await video.play();

    const canvas = document.createElement('canvas');
    canvas.width = ledCount;
    canvas.height = 4;
    const ctx = canvas.getContext('2d', { willReadFrequently: true });

    await fetch(esp + '/api/led?effect=ambience');
    timer = setInterval(async () => {
      if (video.readyState < 2) return;
      ctx.drawImage(video, 0, 0, canvas.width, canvas.height);
      const data = ctx.getImageData(0, 0, canvas.width, canvas.height).data;
      const rgb = [];
      for (let x = 0; x < canvas.width; x++) {
        let r = 0, g = 0, b = 0;
        for (let y = 0; y < canvas.height; y++) {
          const i = (y * canvas.width + x) * 4;
          r += data[i];
          g += data[i + 1];
          b += data[i + 2];
        }
        rgb.push(Math.round(r / canvas.height), Math.round(g / canvas.height), Math.round(b / canvas.height));
      }
      try {
        const response = await fetch(esp + '/api/led/ambience', {
          method: 'POST',
          headers: { 'Content-Type': 'text/plain' },
          body: rgb.join(',')
        });
        if (!response.ok) throw new Error('HTTP ' + response.status);
        setStatus('Streaming screen ambience to ' + esp);
      } catch (error) {
        setStatus('Streaming failed: ' + error.message);
      }
    }, 70);

    stream.getVideoTracks()[0].addEventListener('ended', stopAmbience);
    setStatus('Screen capture started.');
  } catch (error) {
    setStatus('Screen capture failed: ' + error.message);
  }
}

function stopAmbience() {
  if (timer) {
    clearInterval(timer);
    timer = null;
  }
  if (stream) {
    stream.getTracks().forEach(track => track.stop());
    stream = null;
  }
}

probeEsp();
</script>
</body>
</html>
"""


class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path not in ("/", "/index.html"):
            self.send_error(404)
            return
        body = PAGE.encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, fmt, *args):
        print("%s - %s" % (self.address_string(), fmt % args))


def main():
    parser = argparse.ArgumentParser(description="Serve the ESP32 screen ambience localhost helper.")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8765)
    args = parser.parse_args()
    server = ThreadingHTTPServer((args.host, args.port), Handler)
    print(f"Screen ambience helper: http://{args.host}:{args.port}/")
    server.serve_forever()


if __name__ == "__main__":
    main()
