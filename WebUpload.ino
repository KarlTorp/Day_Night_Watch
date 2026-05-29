// ---- Web upload server ----
// Globals used here (declared in Day_Night_Watch.ino): server, tft, LittleFS
// convertBmpToRaw() is defined in Day_Night_Watch.ino (compiled first).

static const char UPLOAD_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Watch Image Upload</title>
<style>
  body{font-family:sans-serif;max-width:480px;margin:40px auto;padding:0 16px}
  h2{margin-bottom:4px}
  .card{border:1px solid #ddd;border-radius:8px;padding:16px;margin-top:16px}
  .card h3{margin:0 0 12px}
  input[type=file]{width:100%;padding:6px;box-sizing:border-box}
  button{margin-top:12px;width:100%;padding:12px;border:none;border-radius:4px;font-size:15px;cursor:pointer;color:#fff}
  .day-btn{background:#f59e0b}.day-btn:hover{background:#d97706}
  .night-btn{background:#3b82f6}.night-btn:hover{background:#1d4ed8}
  .sched-btn{background:#6b7280}.sched-btn:hover{background:#4b5563}
  .status{margin-top:10px;padding:8px;border-radius:4px;display:none;font-size:14px}
  .ok{background:#d4edda;color:#155724}.err{background:#f8d7da;color:#721c24}
  .preview{width:100%;margin-top:12px;border-radius:4px;border:1px solid #eee;display:none}
</style></head><body>
<h2>Watch Image Upload</h2>

<div class="card">
  <h3>&#9728; Day Image</h3>
  <img id="day-preview" class="preview" src="/preview/day"
       onload="this.style.display='block'" onerror="this.style.display='none'">
  <input type="file" id="day-file" accept=".bmp,image/bmp">
  <small style="color:#888">BMP format required</small>
  <button class="day-btn" onclick="upload('day')">Upload Day</button>
  <div class="status" id="day-status"></div>
</div>

<div class="card">
  <h3>&#9790; Night Image</h3>
  <img id="night-preview" class="preview" src="/preview/night"
       onload="this.style.display='block'" onerror="this.style.display='none'">
  <input type="file" id="night-file" accept=".bmp,image/bmp">
  <small style="color:#888">BMP format required</small>
  <button class="night-btn" onclick="upload('night')">Upload Night</button>
  <div class="status" id="night-status"></div>
</div>

<div class="card">
  <h3>&#9201; Schedule</h3>
  <label style="display:block;margin-bottom:4px;font-size:14px">Day starts</label>
  <input type="time" id="day-time" style="width:100%;padding:6px;box-sizing:border-box">
  <label style="display:block;margin:10px 0 4px;font-size:14px">Night starts</label>
  <input type="time" id="night-time" style="width:100%;padding:6px;box-sizing:border-box">
  <button class="sched-btn" onclick="saveScheduleConfig()">Save Schedule</button>
  <div class="status" id="sched-status"></div>
</div>

<div class="card">
  <h3>Label Position</h3>
  <label style="display:block;margin-bottom:4px;font-size:14px">X (pixels from left)</label>
  <input type="number" id="label-x" min="0" max="239" style="width:100%;padding:6px;box-sizing:border-box">
  <label style="display:block;margin:10px 0 4px;font-size:14px">Y (pixels from top)</label>
  <input type="number" id="label-y" min="0" max="239" style="width:100%;padding:6px;box-sizing:border-box">
  <button class="sched-btn" onclick="saveLabelPosition()">Save Position</button>
  <div class="status" id="labelpos-status"></div>
</div>

<hr style="margin-top:32px">
<h3>Files on device</h3>
<ul id="files"><li>Loading...</li></ul>

<script>
async function upload(which) {
  const file = document.getElementById(which + '-file').files[0];
  const status = document.getElementById(which + '-status');
  if (!file) {
    status.textContent = 'Select a file first';
    status.className = 'status err';
    status.style.display = 'block';
    return;
  }
  status.style.display = 'none';
  const fd = new FormData();
  fd.append('file', file, which + '.bmp');
  try {
    const r = await fetch('/upload', { method: 'POST', body: fd });
    const t = await r.text();
    status.textContent = r.ok ? 'Uploaded and converted: ' + which + '.raw' : 'Error: ' + t;
    status.className = 'status ' + (r.ok ? 'ok' : 'err');
    if (r.ok) {
      loadFiles();
      const img = document.getElementById(which + '-preview');
      img.onload = () => img.style.display = 'block';
      img.onerror = () => img.style.display = 'none';
      img.src = '/preview/' + which + '?' + Date.now();
    }
  } catch (err) {
    status.textContent = 'Upload failed: ' + err;
    status.className = 'status err';
  }
  status.style.display = 'block';
}

async function loadFiles() {
  const r = await fetch('/list');
  const d = await r.json();
  const ul = document.getElementById('files');
  ul.innerHTML = d.length
    ? d.map(f => `<li>${f.name} &mdash; ${f.size} bytes</li>`).join('')
    : '<li>(none)</li>';
}

async function fetchSchedule() {
  try {
    const r = await fetch('/schedule');
    const d = await r.json();
    document.getElementById('day-time').value =
      String(d.dayHour).padStart(2,'0') + ':' + String(d.dayMin).padStart(2,'0');
    document.getElementById('night-time').value =
      String(d.nightHour).padStart(2,'0') + ':' + String(d.nightMin).padStart(2,'0');
  } catch(e) {}
}

async function saveScheduleConfig() {
  const status = document.getElementById('sched-status');
  const dayVal   = document.getElementById('day-time').value;
  const nightVal = document.getElementById('night-time').value;
  if (!dayVal || !nightVal) {
    status.textContent = 'Please set both times';
    status.className = 'status err';
    status.style.display = 'block';
    return;
  }
  const [dh, dm] = dayVal.split(':').map(Number);
  const [nh, nm] = nightVal.split(':').map(Number);
  const body = new URLSearchParams({ dayHour: dh, dayMin: dm, nightHour: nh, nightMin: nm });
  try {
    const r = await fetch('/schedule?' + body, { method: 'POST' });
    status.textContent = r.ok ? 'Schedule saved' : 'Error saving';
    status.className = 'status ' + (r.ok ? 'ok' : 'err');
  } catch(e) {
    status.textContent = 'Save failed: ' + e;
    status.className = 'status err';
  }
  status.style.display = 'block';
}

async function fetchLabelPos() {
  try {
    const r = await fetch('/labelpos');
    const d = await r.json();
    document.getElementById('label-x').value = d.x;
    document.getElementById('label-y').value = d.y;
  } catch(e) {}
}

async function saveLabelPosition() {
  const status = document.getElementById('labelpos-status');
  const x = parseInt(document.getElementById('label-x').value);
  const y = parseInt(document.getElementById('label-y').value);
  if (isNaN(x) || isNaN(y)) {
    status.textContent = 'Enter valid X and Y values';
    status.className = 'status err';
    status.style.display = 'block';
    return;
  }
  const body = new URLSearchParams({ x, y });
  try {
    const r = await fetch('/labelpos?' + body, { method: 'POST' });
    status.textContent = r.ok ? 'Position saved — takes effect next minute' : 'Error saving';
    status.className = 'status ' + (r.ok ? 'ok' : 'err');
  } catch(e) {
    status.textContent = 'Save failed: ' + e;
    status.className = 'status err';
  }
  status.style.display = 'block';
}

loadFiles();
fetchSchedule();
fetchLabelPos();
</script>
</body></html>
)rawliteral";

void handleRoot() {
  server.send_P(200, "text/html", UPLOAD_HTML);
}

void handleList() {
  String json = "[";
  File root = LittleFS.open("/");
  File f = root.openNextFile();
  bool first = true;
  while (f) {
    if (!first) json += ",";
    json += "{\"name\":\"" + String(f.name()) + "\",\"size\":" + f.size() + "}";
    first = false;
    f = root.openNextFile();
  }
  json += "]";
  server.send(200, "application/json", json);
}

void handleUpload() {
  server.send(200, "text/plain", "OK");
}

void handleUploadData() {
  HTTPUpload& upload = server.upload();
  static File uploadFile;

  if (upload.status == UPLOAD_FILE_START) {
    String filename = "/" + upload.filename;
    if (!filename.endsWith(".bmp")) return;
    uploadFile = LittleFS.open(filename, "w");
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (uploadFile) uploadFile.write(upload.buf, upload.currentSize);
  } else if (upload.status == UPLOAD_FILE_END) {
    if (uploadFile) {
      uploadFile.close();
      String bmpPath = "/" + upload.filename;
      String rawPath = bmpPath.substring(0, bmpPath.lastIndexOf('.')) + ".raw";
      convertBmpToRaw(bmpPath.c_str(), rawPath.c_str());
    }
  }
}

void handleNotFound() {
  server.send(404, "text/plain", "Not found");
}

// Serves a stored .raw file as a standard 24-bit BMP so the browser can display it.
// Converts RGB565 → BGR24 on the fly, one row at a time.
void handlePreview(const String& name) {
  String rawPath = "/" + name + ".raw";
  File f = LittleFS.open(rawPath, "r");
  if (!f) {
    server.send(404, "text/plain", "No image stored yet");
    return;
  }

  uint16_t w, h;
  f.read((uint8_t*)&w, 2);
  f.read((uint8_t*)&h, 2);

  // BMP rows must be padded to a multiple of 4 bytes (240*3=720 is already aligned)
  uint32_t rowSize = ((w * 3 + 3) / 4) * 4;
  uint32_t fileSize = 54 + rowSize * h;

  uint8_t hdr[54] = {};
  hdr[0]='B'; hdr[1]='M';
  hdr[2]=fileSize;     hdr[3]=fileSize>>8;     hdr[4]=fileSize>>16;     hdr[5]=fileSize>>24;
  hdr[10]=54;           // pixel data offset
  hdr[14]=40;           // BITMAPINFOHEADER size
  hdr[18]=w; hdr[19]=w>>8;
  hdr[22]=h; hdr[23]=h>>8; // positive height = bottom-up row order
  hdr[26]=1;            // colour planes
  hdr[28]=24;           // bits per pixel
  uint32_t dataSize = rowSize * h;
  hdr[34]=dataSize; hdr[35]=dataSize>>8; hdr[36]=dataSize>>16; hdr[37]=dataSize>>24;

  // Use raw WiFiClient for reliable binary streaming
  WiFiClient client = server.client();
  client.printf("HTTP/1.1 200 OK\r\nContent-Type: image/bmp\r\nContent-Length: %u\r\nCache-Control: no-cache\r\n\r\n", fileSize);
  client.write(hdr, 54);

  uint16_t rgb565Buf[240];
  uint8_t  bgrBuf[720]; // 240 * 3, already 4-byte aligned
  memset(bgrBuf, 0, sizeof(bgrBuf));

  // BMP stores rows bottom-to-top; our raw file is top-to-bottom, so iterate in reverse
  for (int32_t row = (int32_t)h - 1; row >= 0; row--) {
    f.seek(4 + (uint32_t)row * w * 2);
    f.read((uint8_t*)rgb565Buf, w * 2);
    for (uint16_t col = 0; col < w; col++) {
      uint16_t c = rgb565Buf[col];
      bgrBuf[col*3]   = (c << 3) & 0xF8; // B
      bgrBuf[col*3+1] = (c >> 3) & 0xFC; // G
      bgrBuf[col*3+2] = (c >> 8) & 0xF8; // R
    }
    client.write(bgrBuf, rowSize);
  }
  f.close();
}

void handleGetSchedule() {
  String json = "{";
  json += "\"dayHour\":"   + String(schedule.dayHour)   + ",";
  json += "\"dayMin\":"    + String(schedule.dayMin)    + ",";
  json += "\"nightHour\":" + String(schedule.nightHour) + ",";
  json += "\"nightMin\":"  + String(schedule.nightMin);
  json += "}";
  server.send(200, "application/json", json);
}

void handleSetSchedule() {
  if (server.hasArg("dayHour") && server.hasArg("dayMin") &&
      server.hasArg("nightHour") && server.hasArg("nightMin")) {
    schedule.dayHour   = constrain(server.arg("dayHour").toInt(),   0, 23);
    schedule.dayMin    = constrain(server.arg("dayMin").toInt(),    0, 59);
    schedule.nightHour = constrain(server.arg("nightHour").toInt(), 0, 23);
    schedule.nightMin  = constrain(server.arg("nightMin").toInt(),  0, 59);
    saveSchedule();
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Missing parameters");
  }
}

void handleGetLabelPos() {
  String json = "{\"x\":" + String(labelX) + ",\"y\":" + String(labelY) + "}";
  server.send(200, "application/json", json);
}

void handleSetLabelPos() {
  if (server.hasArg("x") && server.hasArg("y")) {
    labelX = (uint16_t)constrain(server.arg("x").toInt(), 0, 239);
    labelY = (uint16_t)constrain(server.arg("y").toInt(), 0, 239);
    saveLabelPos();
    timeLabel.setPosition(labelX, labelY);
    timeBackground.setPosition(labelX, labelY);
    backgroundReady = false; // trigger re-capture on next loop iteration
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Missing parameters");
  }
}

void startWebServer() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/list", HTTP_GET, handleList);
  server.on("/upload", HTTP_POST, handleUpload, handleUploadData);
  server.on("/preview/day",   HTTP_GET, []() { handlePreview("day"); });
  server.on("/preview/night", HTTP_GET, []() { handlePreview("night"); });
  server.on("/schedule", HTTP_GET,  handleGetSchedule);
  server.on("/schedule", HTTP_POST, handleSetSchedule);
  server.on("/labelpos", HTTP_GET,  handleGetLabelPos);
  server.on("/labelpos", HTTP_POST, handleSetLabelPos);
  server.onNotFound(handleNotFound);
  server.begin();
}
