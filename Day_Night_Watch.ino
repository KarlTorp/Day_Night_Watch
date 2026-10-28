#include <WiFiManager.h>
#include <WebServer.h>

#include <time.h>
#include <ezTime.h>
#include "graphics.h"
#include <LittleFS.h>

TFT_eSPI tft = TFT_eSPI();
Timezone timeZone;
WebServer server(80);

// Runtime position — overwritten by loadLabelPos() on boot
uint16_t labelX = 15;
uint16_t labelY = 120;

const int MAX_TIME_BG_W = 140;
const int MAX_TIME_BG_H = 60;
static uint16_t timeBackgroundBuffer[MAX_TIME_BG_W * MAX_TIME_BG_H];

PixelBuffer timeBackground(tft, labelX, labelY, MAX_TIME_BG_W, MAX_TIME_BG_H, timeBackgroundBuffer);
Label timeLabel(tft, labelX, labelY, 60, 3, TFT_WHITE);
Label infoLabel(tft, 50, 120, 140, 1, TFT_BLACK);

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
  timeLabel.setPosition(labelX, labelY);
  timeBackground.setPosition(labelX, labelY);

  infoLabel.write("Connecting to WiFi");
  WiFiManager wm; // https://dronebotworkshop.com/wifimanager/
  if(wm.autoConnect())
    infoLabel.write("Connecting to WiFi - OK");
  else
    infoLabel.write("Connecting to WiFi - Fail");
  delay(1000);
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
      infoLabel.write(WiFi.localIP().toString().c_str());
      delay(2000);
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
        if (hasImage)
          timeBackground.draw();
        else
          tft.fillRect(labelX, labelY, MAX_TIME_BG_W, MAX_TIME_BG_H,
                       currentlyDay ? DAY_FALLBACK_COLOR : NIGHT_FALLBACK_COLOR);
        timeLabel.write(timeStr.c_str(), false);
      }
    }
  }
}
