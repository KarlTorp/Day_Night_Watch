// Firmware version — shown on the boot screen and the config web page.
#define FW_VERSION "1.0.2"

#include <WiFiManager.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>

#include <time.h>
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

bool connectedToWiFi = false;
unsigned long lastWiFiUpdate = 0;
bool connectIndex = false;
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

  infoLabel.write("Connecting to WiFi");
  WiFiManager wm; // https://dronebotworkshop.com/wifimanager/
  wm.setHostname(HOSTNAME);
  if(wm.autoConnect())
    infoLabel.write("Connecting to WiFi - OK");
  else
    infoLabel.write("Connecting to WiFi - Fail");
  delay(1000);
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

void updateConnection() {
  if(!connectedToWiFi && millis() - lastWiFiUpdate >= 500)
  {
    lastWiFiUpdate = millis(); 
    if (WiFi.status() != WL_CONNECTED) {
      if (!connectIndex) {
        infoLabel.write("Connecting to WiFi");
        connectIndex = true;
      } else {
        infoLabel.write("Connecting to WiFi...");
        connectIndex = false;
      }
    } else {
      connectedToWiFi = true;
      startWebServer();
      if (MDNS.begin(HOSTNAME)) {
        MDNS.addService("http", "tcp", 80);
        hostLabel.write((String("http://") + HOSTNAME + ".local").c_str());
      }
      infoLabel.write(WiFi.localIP().toString().c_str());
      versionLabel.write("FW " FW_VERSION);
      delay(3000);
      // Fixed POSIX rule for Europe/Copenhagen (CET/CEST) — works offline,
      // unlike setLocation() which needs ezTime's timezone server.
      timeZone.setPosix("CET-1CEST,M3.5.0,M10.5.0/3");
      // Don't block forever; events() in loop() keeps retrying NTP.
      waitForSync(30);
    }
  }
}

void loop() {
  events(); // ezTime: runs periodic NTP resync
  updateConnection();
  if (connectedToWiFi) server.handleClient();
  if (millis() - lastUpdate >= refreshInterval) {
    lastUpdate = millis();
    if (connectedToWiFi && timeStatus() != timeNotSet) {
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
