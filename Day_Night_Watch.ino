#include "day.h"
#include "night.h"

#include <WiFiManager.h>
#include "WIFI.h"

#include <time.h>
#include <ezTime.h>
#include <graphics.h>

TFT_eSPI tft = TFT_eSPI();
Timezone timeZone;

const uint16_t BG_WIDTH = 240;
const uint16_t BG_HEIGHT = 240;
const int TIME_X = 15;
const int TIME_Y = 120;

Label infoLable(tft, 50, 120, 140, 1, TFT_BLACK);
Label timeLabel(tft, TIME_X, TIME_Y, 60, 3, TFT_WHITE);

const int MAX_TIME_BG_W = 140;
const int MAX_TIME_BG_H = 60;
static uint16_t timeBackgroundBuffer[MAX_TIME_BG_W * MAX_TIME_BG_H];
PixelBuffer timeBackground(tft, TIME_X, TIME_Y, MAX_TIME_BG_W, MAX_TIME_BG_H, timeBackgroundBuffer);

const char* ntpServer = "pool.ntp.org";

bool connectedToWiFi = false;
unsigned long lastWiFiUpdate = 0;
bool connectIndex = false;
unsigned long lastUpdate = 0;
unsigned long refreshInterval = 2000;
String currentTime;
uint8_t currentHour = 0;

void setup() {
  tft.begin();
  tft.setRotation(0);
  tft.fillScreen(TFT_WHITE);
  tft.setSwapBytes(true); // Swap the colour byte order when rendering

  // Show both images at startup to verify placcement and colors
  tft.pushImage(0, 0, BG_WIDTH, BG_HEIGHT, nightImg);
  timeLabel.write("12:00", false);

  delay(2000);

  tft.pushImage(0, 0, BG_WIDTH, BG_HEIGHT, dayImg);
  timeLabel.write("12:00", false);

  delay(2000);

  infoLable.write("Connecting to WiFi");
  WiFiManager wm; // https://dronebotworkshop.com/wifimanager/
  if(wm.autoConnect())
    infoLable.write("Connecting to WiFi - OK");
  else
    infoLable.write("Connecting to WiFi - Fail");
  delay(1000);
}

void updateConnection() {
  if(!connectedToWiFi && lastWiFiUpdate + 500 < millis())
  {
    lastWiFiUpdate = millis(); 
    if (WiFi.status() != WL_CONNECTED) {
      if (!connectIndex) {
        infoLable.write("Connecting to WiFi");
        connectIndex = true;
      } else {
        infoLable.write("Connecting to WiFi...");
        connectIndex = false;
      }
    } else {
      connectedToWiFi = true;
      timeZone.setLocation("Europe/Copenhagen");
      waitForSync();
    }
  }
}

void UpdateBackground(uint8_t hour) {
  if(currentHour != hour)
  {
    currentHour = hour;
    if(hour < 6 || hour >= 19) {
      tft.pushImage(0, 0, BG_WIDTH, BG_HEIGHT, nightImg);
      timeBackground.create(nightImg, BG_WIDTH, BG_HEIGHT);
      timeLabel.setTextColor(TFT_WHITE);
    } else {
      tft.pushImage(0, 0, BG_WIDTH, BG_HEIGHT, dayImg);
      timeBackground.create(dayImg, BG_WIDTH, BG_HEIGHT);
      timeLabel.setTextColor(TFT_WHITE);
    }
  }
}

void loop() {
  updateConnection();
  if (millis() > lastUpdate + refreshInterval) {
    lastUpdate = millis();
    if(connectedToWiFi) {
      String timeStr = timeZone.dateTime("H:i");
      if (!timeStr.equals(currentTime)) {
        currentTime = timeStr;
        UpdateBackground(timeZone.hour());
        timeBackground.draw();
        timeLabel.write(timeStr.c_str(), false);
      }
    }
  }
}
