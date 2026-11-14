// ---- Web upload server ----
// Globals used here (declared in Day_Night_Watch.ino): server, tft, LittleFS
// convertBmpToRaw() and MAX_IMG_DIM are defined in graphics.h.

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
  .fw-btn{background:#dc2626}.fw-btn:hover{background:#b91c1c}.fw-btn:disabled{background:#9ca3af;cursor:default}
  .status{margin-top:10px;padding:8px;border-radius:4px;display:none;font-size:14px}
  .ok{background:#d4edda;color:#155724}.err{background:#f8d7da;color:#721c24}
  .preview{width:240px;max-width:100%;margin:12px auto 0;border-radius:50%;border:1px solid #eee;display:none}
</style></head><body>
<h2>Watch Image Upload</h2>
<small style="color:#888">Firmware <span id="fw-version">&hellip;</span></small>

<div class="card">
  <h3>&#9728; Day Image</h3>
  <img id="day-preview" class="preview" src="/preview/day"
       onload="this.style.display='block'" onerror="this.style.display='none'">
  <input type="file" id="day-file" accept="image/*" onchange="prepareImage('day')">
  <small style="color:#888">Any image &mdash; cropped to a centred square and resized to 240&times;240</small>
  <button class="day-btn" onclick="upload('day')">Upload Day</button>
  <div class="status" id="day-status"></div>
</div>

<div class="card">
  <h3>&#9790; Night Image</h3>
  <img id="night-preview" class="preview" src="/preview/night"
       onload="this.style.display='block'" onerror="this.style.display='none'">
  <input type="file" id="night-file" accept="image/*" onchange="prepareImage('night')">
  <small style="color:#888">Any image &mdash; cropped to a centred square and resized to 240&times;240</small>
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
  <h3>Clock</h3>
  <label style="display:block;margin-bottom:4px;font-size:14px">Show time</label>
  <select id="clock-show" style="width:100%;padding:6px;box-sizing:border-box">
    <option value="0">Always</option>
    <option value="1">Only at night</option>
    <option value="2">Only during the day</option>
    <option value="3">Never</option>
  </select>
  <label style="display:block;margin:10px 0 4px;font-size:14px">Font</label>
  <select id="clock-font" style="width:100%;padding:6px;box-sizing:border-box"></select>
  <label style="display:block;margin:10px 0 4px;font-size:14px">Color</label>
  <input type="color" id="time-color" value="#ffffff"
         style="width:100%;height:44px;padding:2px;box-sizing:border-box;border:1px solid #ddd;border-radius:4px">
  <div style="display:flex;gap:12px;margin-top:10px">
    <div style="flex:1">
      <label style="display:block;margin-bottom:4px;font-size:14px">X (pixels from left)</label>
      <input type="number" id="label-x" min="0" max="239" style="width:100%;padding:6px;box-sizing:border-box">
    </div>
    <div style="flex:1">
      <label style="display:block;margin-bottom:4px;font-size:14px">Y (pixels from top)</label>
      <input type="number" id="label-y" min="0" max="239" style="width:100%;padding:6px;box-sizing:border-box">
    </div>
  </div>
  <button class="sched-btn" onclick="saveClock()">Save Clock</button>
  <div class="status" id="clock-status"></div>
</div>

<div class="card">
  <h3>&#8635; Firmware Update</h3>
  <input type="password" id="fw-password" placeholder="Password" autocomplete="current-password"
         style="width:100%;padding:6px;box-sizing:border-box;margin-bottom:8px">
  <input type="file" id="fw-file" accept=".bin">
  <small style="color:#888">Use Day_Night_Watch.ino.bin from Sketch &rarr; Export Compiled Binary (not the merged, bootloader or partitions .bin)</small>
  <button class="fw-btn" id="fw-btn" onclick="uploadFirmware()">Update Firmware</button>
  <div class="status" id="fw-status"></div>
</div>

<div class="card">
  <h3>&#9881; Device</h3>
  <div style="display:flex;gap:12px">
    <button class="sched-btn" onclick="restartWatch()">Restart</button>
    <button class="fw-btn" onclick="resetWifi()">Reset WiFi</button>
  </div>
  <small style="color:#888">Reset WiFi forgets the saved network. The watch then starts its own setup
    network, shown on its screen &mdash; join it to choose a new WiFi.</small>
  <div class="status" id="device-status"></div>
</div>

<hr style="margin-top:32px">
<h3>Files on device</h3>
<ul id="files"><li>Loading...</li></ul>

<script>
// Images are cropped to a centred square and scaled to the display size in the
// browser, then sent as a 24-bit BMP, the only format the watch converts.
const IMG_SIZE = 240;
const prepared = {}; // 'day' / 'night' -> canvas ready to upload

async function prepareImage(which) {
  const file = document.getElementById(which + '-file').files[0];
  const status = document.getElementById(which + '-status');
  delete prepared[which];
  if (!file) return;
  try {
    const src = await createImageBitmap(file);
    const canvas = document.createElement('canvas');
    canvas.width = canvas.height = IMG_SIZE;
    const ctx = canvas.getContext('2d');
    ctx.fillStyle = '#fff'; // background for transparent images
    ctx.fillRect(0, 0, IMG_SIZE, IMG_SIZE);
    const side = Math.min(src.width, src.height);
    ctx.drawImage(src, (src.width - side) / 2, (src.height - side) / 2, side, side,
                  0, 0, IMG_SIZE, IMG_SIZE);
    prepared[which] = canvas;
    const img = document.getElementById(which + '-preview');
    img.src = canvas.toDataURL();
    img.style.display = 'block';
    status.textContent = 'Preview - click Upload to send it to the watch';
    status.className = 'status ok';
  } catch (e) {
    status.textContent = 'Could not read this image';
    status.className = 'status err';
  }
  status.style.display = 'block';
}

function canvasToBmp(canvas) {
  const w = canvas.width, h = canvas.height;
  const px = canvas.getContext('2d').getImageData(0, 0, w, h).data;
  const rowSize = Math.ceil(w * 3 / 4) * 4; // rows are padded to 4 bytes
  const size = 54 + rowSize * h;
  const buf = new ArrayBuffer(size);
  const v = new DataView(buf);
  v.setUint8(0, 0x42); v.setUint8(1, 0x4D); // 'BM'
  v.setUint32(2, size, true);
  v.setUint32(10, 54, true);                // pixel data offset
  v.setUint32(14, 40, true);                // BITMAPINFOHEADER size
  v.setInt32(18, w, true);
  v.setInt32(22, h, true);                  // positive height = bottom-up rows
  v.setUint16(26, 1, true);                 // colour planes
  v.setUint16(28, 24, true);                // bits per pixel
  v.setUint32(34, rowSize * h, true);
  const out = new Uint8Array(buf);
  for (let y = 0; y < h; y++) {
    let o = 54 + (h - 1 - y) * rowSize;
    for (let x = 0; x < w; x++) {
      const i = (y * w + x) * 4;
      out[o++] = px[i + 2]; out[o++] = px[i + 1]; out[o++] = px[i]; // BGR
    }
  }
  return new Blob([buf], { type: 'image/bmp' });
}

async function upload(which) {
  const status = document.getElementById(which + '-status');
  if (!prepared[which]) {
    status.textContent = 'Select an image first';
    status.className = 'status err';
    status.style.display = 'block';
    return;
  }
  status.style.display = 'none';
  const fd = new FormData();
  fd.append('file', canvasToBmp(prepared[which]), which + '.bmp');
  try {
    const r = await fetch('/upload', { method: 'POST', body: fd });
    const t = await r.text();
    status.textContent = r.ok ? 'Uploaded to the watch' : 'Error: ' + t;
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

// The font list comes from the watch, so new fonts only need adding in firmware.
async function fetchClock() {
  try {
    const r = await fetch('/clock');
    const d = await r.json();
    document.getElementById('clock-font').innerHTML =
      d.fonts.map((name, i) => `<option value="${i}">${name}</option>`).join('');
    document.getElementById('clock-font').value = d.font;
    document.getElementById('clock-show').value = d.show;
    document.getElementById('time-color').value = d.color;
  } catch(e) {}
}

// Position and the other clock settings have separate endpoints on the watch;
// the page saves both from one button.
async function saveClock() {
  const status = document.getElementById('clock-status');
  const x = parseInt(document.getElementById('label-x').value);
  const y = parseInt(document.getElementById('label-y').value);
  if (isNaN(x) || isNaN(y)) {
    status.textContent = 'Enter valid X and Y values';
    status.className = 'status err';
    status.style.display = 'block';
    return;
  }
  const clockBody = new URLSearchParams({
    show:  document.getElementById('clock-show').value,
    font:  document.getElementById('clock-font').value,
    color: document.getElementById('time-color').value.substring(1) // drop '#'
  });
  try {
    let r = await fetch('/labelpos?' + new URLSearchParams({ x, y }), { method: 'POST' });
    if (r.ok) r = await fetch('/clock?' + clockBody, { method: 'POST' });
    status.textContent = r.ok ? 'Clock saved' : 'Error saving: ' + await r.text();
    status.className = 'status ' + (r.ok ? 'ok' : 'err');
  } catch(e) {
    status.textContent = 'Save failed: ' + e;
    status.className = 'status err';
  }
  status.style.display = 'block';
}

async function deviceAction(path, question, doneText) {
  if (!confirm(question)) return false;
  const status = document.getElementById('device-status');
  try {
    const r = await fetch(path, { method: 'POST' });
    status.textContent = r.ok ? doneText : 'Error: ' + await r.text();
    status.className = 'status ' + (r.ok ? 'ok' : 'err');
    status.style.display = 'block';
    return r.ok;
  } catch (e) {
    status.textContent = 'Could not reach the watch: ' + e;
    status.className = 'status err';
    status.style.display = 'block';
    return false;
  }
}

async function restartWatch() {
  if (await deviceAction('/restart', 'Restart the watch?',
                         'Restarting - this page reloads in 15 seconds.'))
    setTimeout(() => location.reload(), 15000);
}

function resetWifi() {
  deviceAction('/wifi/reset',
               'Forget the saved WiFi? The watch will go offline until it is set up again.',
               'WiFi forgotten - the watch is restarting into setup mode. Join the network shown on its screen.');
}

// Uses XMLHttpRequest rather than fetch() to get upload progress.
// The password is checked first so a wrong one doesn't cost a full upload.
async function uploadFirmware() {
  const password = document.getElementById('fw-password').value;
  const file = document.getElementById('fw-file').files[0];
  const status = document.getElementById('fw-status');
  const btn = document.getElementById('fw-btn');
  const show = (text, ok) => {
    status.textContent = text;
    status.className = 'status ' + (ok ? 'ok' : 'err');
    status.style.display = 'block';
  };
  if (!password) { show('Enter the password', false); return; }
  if (!file) { show('Select a file first', false); return; }
  // Export Compiled Binary also produces these; only the plain app .bin is valid here
  if (/merged|bootloader|partitions/.test(file.name)) {
    show('Wrong file - use Day_Night_Watch.ino.bin', false);
    return;
  }
  // HTTP Basic auth; encode as UTF-8 first since btoa() only accepts Latin-1
  const auth = 'Basic ' + btoa(String.fromCharCode(...new TextEncoder().encode('admin:' + password)));
  try {
    const r = await fetch('/update/auth', { method: 'POST', headers: { Authorization: auth } });
    if (!r.ok) { show('Wrong password', false); return; }
  } catch (e) {
    show('Could not reach the watch: ' + e, false);
    return;
  }
  const fd = new FormData();
  fd.append('firmware', file, file.name);
  const xhr = new XMLHttpRequest();
  xhr.open('POST', '/update');
  xhr.setRequestHeader('Authorization', auth);
  xhr.upload.onprogress = e => {
    if (e.lengthComputable) show('Uploading... ' + Math.round(e.loaded / e.total * 100) + '%', true);
  };
  xhr.onload = () => {
    if (xhr.status === 200) {
      show('Update complete - restarting. This page reloads in 15 seconds.', true);
      setTimeout(() => location.reload(), 15000);
    } else {
      show('Update failed: ' + xhr.responseText, false);
      btn.disabled = false;
    }
  };
  xhr.onerror = () => { show('Upload failed - connection lost', false); btn.disabled = false; };
  btn.disabled = true;
  xhr.send(fd);
}

async function fetchVersion() {
  try {
    const r = await fetch('/version');
    document.getElementById('fw-version').textContent = await r.text();
  } catch(e) {}
}

fetchVersion();
loadFiles();
fetchSchedule();
fetchLabelPos();
fetchClock();
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

// Set by handleUploadData() once the file is received; read by handleUpload(),
// which the server calls after the upload has finished.
static bool uploadConverted = false;

void handleUpload() {
  if (uploadConverted)
    server.send(200, "text/plain", "OK");
  else
    server.send(400, "text/plain", "Invalid image - must be a 24-bit BMP, max 240x240");
}

void handleUploadData() {
  HTTPUpload& upload = server.upload();
  static File uploadFile;

  if (upload.status == UPLOAD_FILE_START) {
    uploadConverted = false;
    if (upload.filename != "day.bmp" && upload.filename != "night.bmp") return;
    uploadFile = LittleFS.open("/" + upload.filename, "w");
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (uploadFile) uploadFile.write(upload.buf, upload.currentSize);
  } else if (upload.status == UPLOAD_FILE_END) {
    if (uploadFile) {
      uploadFile.close();
      String bmpPath = "/" + upload.filename;
      String rawPath = bmpPath.substring(0, bmpPath.lastIndexOf('.')) + ".raw";
      uploadConverted = convertBmpToRaw(bmpPath.c_str(), rawPath.c_str());
      if (uploadConverted) backgroundReady = false; // redraw with the new image on next loop iteration
    }
  }
}

// Firmware update (OTA). The new image is written to the inactive app
// partition and only activated if it validates, so a bad upload leaves the
// current firmware running. LittleFS (images, settings) is not touched.
// Protected by HTTP Basic auth; the password is OTA_PASSWORD in secrets.h.
const char* OTA_USER = "admin";
static bool firmwareAuthorized = false;
static bool firmwareUpdated = false;

// 401 is sent without a WWW-Authenticate header so the browser shows the
// page's own message instead of popping up its login dialog.
void handleFirmwareAuth() {
  if (server.authenticate(OTA_USER, OTA_PASSWORD))
    server.send(200, "text/plain", "OK");
  else
    server.send(401, "text/plain", "Wrong password");
}

void handleFirmwareUpdate() {
  if (!firmwareAuthorized) {
    server.send(401, "text/plain", "Wrong password");
  } else if (firmwareUpdated) {
    server.send(200, "text/plain", "OK");
    delay(500); // let the response reach the browser before rebooting
    ESP.restart();
  } else {
    server.send(400, "text/plain", Update.errorString());
  }
}

void handleFirmwareUpdateData() {
  HTTPUpload& upload = server.upload();

  if (upload.status == UPLOAD_FILE_START) {
    firmwareUpdated = false;
    // Headers are parsed before the body, so this rejects before anything is flashed
    firmwareAuthorized = server.authenticate(OTA_USER, OTA_PASSWORD);
    if (!firmwareAuthorized) return;
    infoLabel.write("Updating firmware...");
    Update.begin(UPDATE_SIZE_UNKNOWN);
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (Update.isRunning()) Update.write(upload.buf, upload.currentSize);
  } else if (upload.status == UPLOAD_FILE_END) {
    if (!firmwareAuthorized) return;
    firmwareUpdated = Update.end(true); // true = size is whatever was received
    if (!firmwareUpdated) {
      infoLabel.write("Firmware update failed");
      backgroundReady = false; // redraw the normal screen on next loop iteration
    }
  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    if (!firmwareAuthorized) return;
    Update.abort();
    backgroundReady = false;
  }
}

// The response is sent before acting so the browser gets a reply.
void handleRestart() {
  server.send(200, "text/plain", "OK");
  delay(500);
  ESP.restart();
}

// Forgets the saved WiFi; on reboot WiFiManager starts its setup network.
void handleWifiReset() {
  server.send(200, "text/plain", "OK");
  delay(500);
  WiFiManager wm;
  wm.resetSettings();
  ESP.restart();
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
  if (w > MAX_IMG_DIM || h > MAX_IMG_DIM) {
    f.close();
    server.send(500, "text/plain", "Stored image is too large");
    return;
  }

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
    timeText.setPosition(labelX, labelY);
    timeBackground.setPosition(labelX, labelY);
    backgroundReady = false; // trigger re-capture on next loop iteration
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Missing parameters");
  }
}

// The web page uses #rrggbb; the display uses RGB565, so the colour read back
// may differ slightly from the one picked (lowest bits of each channel are lost).
void handleGetClock() {
  uint8_t r = (timeColor >> 11) << 3;
  uint8_t g = ((timeColor >> 5) & 0x3F) << 2;
  uint8_t b = (timeColor & 0x1F) << 3;
  char color[8];
  snprintf(color, sizeof(color), "#%02x%02x%02x", r, g, b);

  String json = "{\"show\":" + String(showTime) + ",\"font\":" + String(clockFont) +
                ",\"color\":\"" + color + "\",\"fonts\":[";
  for (uint8_t i = 0; i < CLOCK_FONT_COUNT; i++) {
    if (i) json += ",";
    json += "\"" + String(CLOCK_FONTS[i].name) + "\"";
  }
  json += "]}";
  server.send(200, "application/json", json);
}

void handleSetClock() {
  String hex = server.arg("color");
  long show = server.arg("show").toInt();
  long font = server.arg("font").toInt();
  if (hex.length() != 6 || strspn(hex.c_str(), "0123456789abcdefABCDEF") != 6) {
    server.send(400, "text/plain", "Expected color as rrggbb");
    return;
  }
  if (!server.hasArg("show") || show < SHOW_ALWAYS || show > SHOW_NEVER ||
      !server.hasArg("font") || font < 0 || font >= CLOCK_FONT_COUNT) {
    server.send(400, "text/plain", "Invalid show or font value");
    return;
  }
  uint32_t rgb = strtoul(hex.c_str(), nullptr, 16);
  timeColor = tft.color565(rgb >> 16, (rgb >> 8) & 0xFF, rgb & 0xFF);
  showTime  = (uint8_t)show;
  clockFont = (uint8_t)font;
  saveClockSettings();
  timeText.setColor(timeColor);
  applyClockFont();
  backgroundReady = false; // full redraw on next loop iteration clears the old time
  server.send(200, "text/plain", "OK");
}

void startWebServer() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/list", HTTP_GET, handleList);
  server.on("/version", HTTP_GET, []() { server.send(200, "text/plain", FW_VERSION); });
  server.on("/upload", HTTP_POST, handleUpload, handleUploadData);
  server.on("/preview/day",   HTTP_GET, []() { handlePreview("day"); });
  server.on("/preview/night", HTTP_GET, []() { handlePreview("night"); });
  server.on("/schedule", HTTP_GET,  handleGetSchedule);
  server.on("/schedule", HTTP_POST, handleSetSchedule);
  server.on("/labelpos", HTTP_GET,  handleGetLabelPos);
  server.on("/labelpos", HTTP_POST, handleSetLabelPos);
  server.on("/clock", HTTP_GET,  handleGetClock);
  server.on("/clock", HTTP_POST, handleSetClock);
  server.on("/update", HTTP_POST, handleFirmwareUpdate, handleFirmwareUpdateData);
  server.on("/update/auth", HTTP_POST, handleFirmwareAuth);
  server.on("/restart", HTTP_POST, handleRestart);
  server.on("/wifi/reset", HTTP_POST, handleWifiReset);
  server.onNotFound(handleNotFound);
  server.begin();
}
