// Firmware version — shown on the boot screen and the config web page.
#define FW_VERSION "1.1.3"

#include <WiFiManager.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>

#include <time.h>
#include <sys/time.h>
#include <ezTime.h>
#include "graphics.h"
#include "secrets.h"
#include <LittleFS.h>

TFT_eSPI tft = TFT_eSPI();
Timezone timeZone;
WebServer server(80);

// Web UI is reachable at http://watch.local (mDNS); also shown as the router's device name
const char* HOSTNAME = "watch";

// Runtime position — overwritten by loadLabelPos() on boot
uint16_t labelX = 15;
uint16_t labelY = 120;
// Fonts selectable for the clock on the web page. Order matters: the index is
// what's saved, so add new fonts at the end.
const ClockFont CLOCK_FONTS[] = {
  {"Classic",    1, nullptr,              3},
  {"Large",      6, nullptr,              1},
  {"Digital",    7, nullptr,              1},
  {"Sans Bold",  0, &FreeSansBold24pt7b,  1},
  {"Serif Bold", 0, &FreeSerifBold24pt7b, 1},
  {"Mono Bold",  0, &FreeMonoBold24pt7b,  1},
};
const uint8_t CLOCK_FONT_COUNT = sizeof(CLOCK_FONTS) / sizeof(CLOCK_FONTS[0]);

// When the clock is shown. Values are saved and used by the web page.
enum ShowTime : uint8_t { SHOW_ALWAYS = 0, SHOW_AT_NIGHT = 1, SHOW_DURING_DAY = 2, SHOW_NEVER = 3 };

// Clock settings — overwritten by loadClockSettings() on boot
uint16_t timeColor = TFT_WHITE; // RGB565
uint8_t clockFont = 0;          // index into CLOCK_FONTS
uint8_t showTime = SHOW_ALWAYS;

// Background behind the clock, saved so it can be restored before each redraw.
// Sized for the largest font; the area actually used follows the selected font.
const int TIME_BG_MAX_W = 240;
const int TIME_BG_MAX_H = 80;
static uint16_t timeBackgroundBuffer[TIME_BG_MAX_W * TIME_BG_MAX_H];

PixelBuffer timeBackground(tft, labelX, labelY, TIME_BG_MAX_W, TIME_BG_MAX_H, timeBackgroundBuffer);
ClockText timeText(tft, labelX, labelY, CLOCK_FONTS[0], timeColor);
Label infoLabel(tft, 50, 120, 140, 1, TFT_BLACK);
Label hostLabel(tft, 50, 132, 140, 1, TFT_BLACK);
Label versionLabel(tft, 50, 144, 140, 1, TFT_BLACK);
Label noteLabel(tft, 50, 156, 140, 1, TFT_BLACK);

// WiFi start-up: with a saved network, keep retrying it this long (covers a
// router that boots slower than the watch after a power cut) before opening
// the setup network. The setup network then closes after its own timeout and
// the watch goes back to retrying, so it never gets stuck in setup.
const unsigned long WIFI_RETRY_MS        = 5 * 60 * 1000UL;
const unsigned long WIFI_SETUP_TIMEOUT_S = 5 * 60;
const unsigned long WIFI_REBEGIN_MS      = 20 * 1000UL; // restart the connection attempt this often
bool wifiRetryFailed = false; // true while the setup network is open because the saved WiFi failed

// Fixed POSIX rule for Europe/Copenhagen (CET/CEST) — works offline,
// unlike setLocation() which needs ezTime's timezone server.
const char* TZ_RULE = "CET-1CEST,M3.5.0,M10.5.0/3";
// Kept system-clock times before this are treated as "not set" (2024-01-01 UTC)
const time_t MIN_VALID_TIME = 1704067200;

bool networkStarted = false;  // web server and mDNS running (stay up across WiFi drop-outs)
bool clockKept = false;       // time survived a restart, so the clock runs before WiFi connects
unsigned long lastWiFiUpdate = 0;
unsigned long lastWiFiBegin = 0;
time_t lastSavedSync = 0;     // last NTP sync copied to the system clock
unsigned long lastUpdate = 0;
unsigned long refreshInterval = 2000;
String currentTime;
bool currentlyDay = true;
bool backgroundReady = false;
bool hasImage = false; // false when the current day/night .raw is missing — a solid colour is used instead
bool firstUpdate = true;

const uint16_t DAY_FALLBACK_COLOR   = TFT_SKYBLUE;
const uint16_t NIGHT_FALLBACK_COLOR = TFT_NAVY;

void setup() {
  tft.begin();
  tft.setRotation(0);
  tft.fillScreen(TFT_WHITE);
  tft.setSwapBytes(true); // Swap the colour byte order when rendering*/

  // true = format on first use if needed. On failure we carry on with defaults.
  if (!LittleFS.begin(true)) {
    infoLabel.write("Storage error");
    delay(2000);
  }
  loadSchedule();
  loadLabelPos();
  loadClockSettings();
  timeText.setPosition(labelX, labelY);
  timeText.setColor(timeColor);
  applyClockFont();
  timeBackground.setPosition(labelX, labelY);

  timeZone.setPosix(TZ_RULE);
  clockKept = restoreClock();

  WiFi.setHostname(HOSTNAME);
  WiFi.mode(WIFI_STA); // needed before getWiFiIsSaved() can read the saved network
  WiFiManager wm;
  if (clockKept && wm.getWiFiIsSaved()) {
    // Restarted with the time intact: show the clock straight away and let
    // loop() connect in the background.
    WiFi.begin();
    lastWiFiBegin = millis();
  } else {
    connectWiFi();
    infoLabel.write("Connecting to WiFi - OK");
    delay(1000);
  }
}

// The ESP32's system clock keeps running through software restarts (Restart
// button, firmware update, Reset WiFi, crashes) but not through power loss.
// Returns true if it still holds a valid time, which is then given to ezTime.
bool restoreClock() {
  struct timeval tv;
  gettimeofday(&tv, nullptr);
  if (tv.tv_sec < MIN_VALID_TIME) return false;
  UTC.setTime(tv.tv_sec, tv.tv_usec / 1000);
  return true;
}

// Copies ezTime's clock to the system clock after each NTP sync, so it
// survives the next software restart.
void saveClock() {
  if (timeStatus() != timeSet || lastNtpUpdateTime() == lastSavedSync) return;
  lastSavedSync = lastNtpUpdateTime();
  time_t t = UTC.now();
  struct timeval tv = { t, (suseconds_t)UTC.ms() * 1000 };
  settimeofday(&tv, nullptr);
}

// Blocks until connected. Saved network: retry it for WIFI_RETRY_MS, then open
// the setup network for WIFI_SETUP_TIMEOUT_S, and repeat. No saved network
// (first use or after Reset WiFi): wait in the setup network until configured.
void connectWiFi() {
  WiFiManager wm; // https://dronebotworkshop.com/wifimanager/
  wm.setHostname(HOSTNAME);
  wm.setAPCallback(onWifiSetupStarted);
  // The WiFi driver must be started before getWiFiIsSaved() can read the saved
  // network on ESP32; otherwise it always reports nothing saved.
  WiFi.mode(WIFI_STA);

  while (true) {
    if (wm.getWiFiIsSaved()) {
      if (retrySavedWiFi(wm.getWiFiSSID())) return;
      wifiRetryFailed = true;
      wm.setConfigPortalTimeout(WIFI_SETUP_TIMEOUT_S);
    } else {
      wifiRetryFailed = false;
      wm.setConfigPortalTimeout(0); // nothing to retry, so no timeout
    }
    if (wm.startConfigPortal()) return; // new WiFi saved and connected
    // Setup timed out without changes: clear its text and retry the saved WiFi
    tft.fillScreen(TFT_WHITE);
  }
}

// Tries the saved WiFi for up to WIFI_RETRY_MS. Returns true once connected.
bool retrySavedWiFi(const String& ssid) {
  hostLabel.write(ssid.c_str());
  WiFi.mode(WIFI_STA);
  WiFi.begin(); // no arguments = use the saved network
  unsigned long start = millis();
  unsigned long lastBegin = start;
  uint8_t dots = 0;
  while (millis() - start < WIFI_RETRY_MS) {
    if (WiFi.status() == WL_CONNECTED) return true;
    if (millis() - lastBegin >= WIFI_REBEGIN_MS) {
      WiFi.disconnect(); // keeps the saved network
      WiFi.begin();
      lastBegin = millis();
    }
    dots = (dots + 1) % 4;
    infoLabel.write((String("Connecting to WiFi") + String("...").substring(0, dots)).c_str());
    delay(500);
  }
  return false;
}

// Defined in WebUpload.ino. Declared here because Arduino's automatic
// prototypes can't be relied on for that file (its HTML/JS raw string confuses the scanner).
void startWebServer();

// Runs when the setup network starts: after the saved WiFi failed, or when
// none is saved (first use or after Reset WiFi).
void onWifiSetupStarted(WiFiManager* wm) {
  String ssid = wm->getConfigPortalSSID();
  tft.fillScreen(TFT_WHITE);
  if (wifiRetryFailed) {
    // Lines kept to ~23 characters to fit the round display
    infoLabel.write("WiFi connection failed.");
    hostLabel.write(("Join " + ssid).c_str());
    versionLabel.write("to set up WiFi, or");
    noteLabel.write("restart to try again.");
  } else {
    infoLabel.write("Join WiFi network:");
    hostLabel.write(ssid.c_str());
  }
}

// Selects the clock font and resizes the saved background area to match it.
void applyClockFont() {
  timeText.setFont(CLOCK_FONTS[clockFont]);
  timeBackground.setSize(min<uint16_t>(timeText.width(), TIME_BG_MAX_W), timeText.height());
}

bool isTimeShown(bool day) {
  switch (showTime) {
    case SHOW_AT_NIGHT:   return !day;
    case SHOW_DURING_DAY: return day;
    case SHOW_NEVER:      return false;
    default:              return true;
  }
}

// Keeps WiFi connected without blocking the clock: retries the saved network
// while it's down, and starts the web server and mDNS on the first connection.
void updateConnection() {
  if (millis() - lastWiFiUpdate < 500) return;
  lastWiFiUpdate = millis();

  if (WiFi.status() != WL_CONNECTED) {
    if (millis() - lastWiFiBegin >= WIFI_REBEGIN_MS) {
      WiFi.disconnect(); // keeps the saved network
      WiFi.begin();
      lastWiFiBegin = millis();
    }
    return;
  }
  if (networkStarted) return;

  networkStarted = true;
  startWebServer();
  bool mdnsOk = MDNS.begin(HOSTNAME);
  if (mdnsOk) MDNS.addService("http", "tcp", 80);

  if (clockKept) return; // clock already on screen — don't cover it with the info screen

  if (mdnsOk) hostLabel.write((String("http://") + HOSTNAME + ".local").c_str());
  infoLabel.write(WiFi.localIP().toString().c_str());
  versionLabel.write("FW " FW_VERSION);
  delay(3000);
  // Don't block forever; events() in loop() keeps retrying NTP.
  waitForSync(30);
}

void loop() {
  events(); // ezTime: runs periodic NTP resync (retries every 20 s while offline)
  updateConnection();
  saveClock();
  if (networkStarted) server.handleClient();
  if (firstUpdate || millis() - lastUpdate >= refreshInterval) {
    lastUpdate = millis();
    // Once set, ezTime keeps counting on its own, so the clock runs without WiFi
    if (timeStatus() != timeNotSet) {
      String timeStr = timeZone.dateTime("H:i");
      if (!timeStr.equals(currentTime) || firstUpdate || !backgroundReady) {
        currentTime = timeStr;
        bool day = isDayTime(timeZone.hour(), timeZone.minute());
        if (day != currentlyDay || !backgroundReady) {
          firstUpdate = false;
          currentlyDay = day;
          backgroundReady = true;
          const char* imgPath = day ? "/day.raw" : "/night.raw";
          hasImage = drawRaw(tft, imgPath, 0, 0);
          if (hasImage)
            timeBackground.captureFromRaw(imgPath);
          else
            tft.fillScreen(day ? DAY_FALLBACK_COLOR : NIGHT_FALLBACK_COLOR);
        }
        // A full redraw above already cleared any old time, so hiding needs no extra work
        if (isTimeShown(currentlyDay)) {
          if (hasImage)
            timeBackground.draw();
          else
            tft.fillRect(labelX, labelY, timeBackground.width(), timeBackground.height(),
                         currentlyDay ? DAY_FALLBACK_COLOR : NIGHT_FALLBACK_COLOR);
          timeText.write(timeStr.c_str());
        }
      }
    }
  }
}
