#include "sun.h"
#include "moon.h"

#include <WiFiManager.h>
#ifdef ESP8266 
#include <ESP8266WiFi.h>
#else
#include "WIFI.h"
#endif
#include <time.h>
#include <ScreenGFX.h>
#include "Label.h"
#include <ezTime.h>
#include <TFT_eSPI.h> 

TFT_eSPI tft = TFT_eSPI();
ScreenGFX screenGFX(tft);
Timezone timeZone;

Label infoLable(screenGFX, 10, 120, 140, 1, TFT_BLACK);
Label timeLabel(screenGFX, 37, 151, 60, 2, TFT_BLACK);

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
      tft.pushImage(0, 0, moonWidth, moonHeight, moon);
      timeLabel.setTextColor(TFT_WHITE);
      timeLabel.setBackgroundColor(0x8C71);
    } else {
      tft.pushImage(0, 0, sunWidth, sunHeight, sun);
      timeLabel.setTextColor(TFT_BLACK);
      timeLabel.setBackgroundColor(TFT_WHITE);
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
          timeLabel.write(timeStr.c_str());
        }
      }
    }
}
