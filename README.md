# Day_Night_Watch

A round TFT clock for kids that shows whether it is **daytime or nighttime**. It displays one picture during the day and another at night (for example a sun and a moon), with the time drawn on top. Pictures, schedule and clock style are set from a web page on your phone or computer.

## Features

- **Day / night pictures.** Upload any picture from the web page. It is cropped to a centred square and resized to 240×240 in the browser. If no picture has been uploaded, a solid colour is shown (sky blue by day, navy at night).
- **Day/night schedule.** Set when the day starts and when the night starts. Either period may span midnight.
- **Clock settings:**
  - When to show the time: always, at night only, during the day only, or never.
  - Font: Classic, Large, Digital, Sans Bold, Serif Bold or Mono Bold.
  - Text colour.
  - X/Y position on the screen.
- **Accurate time.** The time comes from the internet (NTP) and uses the Danish time zone (CET/CEST), with daylight saving switched automatically.
- **Keeps time without WiFi.**
  - If WiFi drops, the clock keeps counting. The watch tries to reconnect every 20 s and syncs the time again once it is back online.
  - After a software restart (Restart button, firmware update, crash), the time is kept and the clock shows straight away. A power cut loses the time; see [Start-up and WiFi behaviour](#start-up-and-wifi-behaviour).
- **WiFi setup without code changes.** If the watch has no saved network, or can't reach it, it opens its own setup network where you choose your WiFi (WiFiManager).
- **Firmware update from the web page,** protected by a password.
- **Device controls:** Restart and Reset WiFi buttons on the web page.
- **Reachable at `http://watch.local`** (mDNS), so you don't need to know the IP address.
- **Firmware version** shown on the boot screen and on the web page.

## Hardware

| Part | Notes |
|---|---|
| ESP32-C3 Super Mini | Any ESP32-C3 with 4 MB flash should work if you adjust the pins. |
| 1.28" round TFT, 240×240, GC9A01 driver | SPI interface. |

### Wiring

| Display pin | ESP32-C3 GPIO |
|---|---|
| SCL / SCLK | 4 |
| SDA / MOSI | 6 |
| CS | 7 |
| DC | 9 |
| RST | 10 |
| VCC | 3V3 |
| GND | GND |

GPIO 5 is set as MISO in the display config, but it isn't connected. The display needs no MISO line.

> Don't use GPIO 12–17 on the ESP32-C3. They are connected to the flash chip, and using them makes the board reboot over and over (`rst:0x8 (TG1WDT_SYS_RST)` in the serial log).

## Software dependencies

### Board package

- **esp32 by Espressif Systems**, version 3.x (developed with 3.3.7). Install it from the Arduino IDE Boards Manager.

### Libraries

Install these from the Arduino IDE Library Manager:

| Library | Tested version | Used for |
|---|---|---|
| TFT_eSPI (Bodmer) | 2.5.43 | Display driver and fonts |
| WiFiManager (tzapu) | 2.0.17 | WiFi setup network |
| ezTime (ropg) | 0.8.3 | NTP time sync and time zone |

These come with the ESP32 board package and need no install: `WebServer`, `ESPmDNS`, `Update`, `LittleFS` and `WiFi`.

## Setup

### 1. Configure TFT_eSPI for this display

TFT_eSPI takes its pin and driver settings from its own library folder, not from the sketch.

1. In `Documents/Arduino/libraries/TFT_eSPI/`, create `User_Setup_GC9A01_C3_mini.h`:

   ```cpp
   #define USER_SETUP_INFO "User_Setup"
   #define USER_SETUP_ID 70

   #define GC9A01_DRIVER

   #define TFT_CS   7
   #define TFT_MOSI 6
   #define TFT_MISO 5   // not connected
   #define TFT_SCLK 4
   #define TFT_DC   9
   #define TFT_RST  10

   #define LOAD_GLCD
   #define LOAD_FONT2
   #define LOAD_FONT4
   #define LOAD_FONT6
   #define LOAD_FONT7
   #define LOAD_FONT8
   #define LOAD_GFXFF   // required: the Sans/Serif/Mono clock fonts are FreeFonts
   #define SMOOTH_FONT

   #define SPI_FREQUENCY      40000000
   #define SPI_READ_FREQUENCY 6000000
   ```

2. In `User_Setup_Select.h`, make sure this is the **only** active (not commented-out) include for ESP32:

   ```cpp
   #include <User_Setup_GC9A01_C3_mini.h>
   ```

   If a setup for another board is also active, for example a CYD setup, the build may still compile but the board reboots constantly.

> Updating TFT_eSPI can overwrite `User_Setup_Select.h`. Check it again after a library update.

### 2. Create `secrets.h`

The firmware update password lives in `secrets.h` in the sketch folder:

```cpp
#pragma once

// Required to install firmware from the config web page
#define OTA_PASSWORD "choose-a-password"
```

`secrets.h` is listed in `.gitignore`. Don't commit your real password.

### 3. Arduino IDE settings

Under **Tools**:

| Setting | Value |
|---|---|
| Board | ESP32C3 Dev Module |
| USB CDC On Boot | Enabled |
| Flash Size | 4MB (32Mb) |
| Partition Scheme | **Custom** |

The sketch folder contains `partitions.csv`, which the ESP32 core uses automatically:

| Partition | Size | Purpose |
|---|---|---|
| app0 / app1 | 1.5 MB each | Two firmware slots, so web updates can work |
| spiffs | 896 KB | LittleFS: pictures and settings |
| nvs, otadata, coredump | small | WiFi credentials, update state, crash dumps |

Choose **Custom** so the IDE's size check uses the 1.5 MB slot rather than the default 1.25 MB.

> The partition table can only change through a **USB upload**. A web firmware update can't move partitions. If you upload once with a different scheme, the web update fails with "Partition Could Not be Found" until you upload over USB with **Custom** again.

### 4. Upload

Connect the board over USB and click **Upload**. The first boot formats the LittleFS storage, which takes a few seconds.

### 5. Connect the watch to WiFi

1. On first boot the screen shows **"Join WiFi network:"** followed by the name of the setup network.
2. Join that network with your phone. A setup page opens; if it doesn't, browse to `http://192.168.4.1`.
3. Choose your WiFi network and enter its password.
4. The watch connects. For 3 seconds it shows its IP address, `http://watch.local` and the firmware version, then starts the clock.

## Using the web page

Open **http://watch.local**, or the IP address shown on the boot screen. mDNS (`.local` names) works on iOS, macOS, Windows 10+ and most Linux systems. Some Android versions need the IP address.

| Section | What it does |
|---|---|
| **Day / Night** | Choose a picture for each period. It is cropped, resized and converted to BMP in the browser, then uploaded. The preview shows what the watch displays. |
| **Schedule** | The time day starts and the time night starts. |
| **Clock** | Show time (Always / At night / During day / Never), font, colour and X/Y position. Click **Save Clock** to apply. |
| **Firmware Update** | Install a new firmware `.bin` file. Requires the `OTA_PASSWORD` from `secrets.h`. The user name is fixed to `admin`. |
| **Device** | **Restart** reboots the watch. **Reset WiFi** forgets the saved network and reopens the WiFi setup network. |
| **Files** | Lists the files stored on the watch. |

### Firmware update over WiFi

1. Bump `FW_VERSION` at the top of `Day_Night_Watch.ino`. That way you can see on the web page that the update took effect.
2. In Arduino IDE, choose **Sketch → Export Compiled Binary**. The file goes in the sketch's `build/` folder.
3. On the web page, enter the password and select **`Day_Night_Watch.ino.bin`**.
   - Don't select `.merged.bin`, `.bootloader.bin` or `.partitions.bin`. The page rejects them.
4. The watch installs the update, restarts and keeps the time. The page reloads after about 15 s.

## Start-up and WiFi behaviour

The watch has no buttons, so it recovers on its own:

| Situation | What happens |
|---|---|
| **No saved WiFi** (first use, or after Reset WiFi) | Opens the setup network and waits until WiFi is configured. |
| **Power-on with saved WiFi** | Tries the saved network for up to 5 minutes. This covers a router that starts slower than the watch after a power cut. |
| **Saved WiFi still unreachable after 5 min** | Shows "WiFi connection failed. Join *setup network* to set up WiFi, or restart to try again." and opens the setup network for 5 minutes. If nothing is changed, it goes back to retrying the saved network, then the setup network again, and so on. |
| **Software restart** (Restart button, firmware update, crash) | The ESP32's internal clock keeps the time, so the clock shows at once and WiFi connects in the background. The IP screen is skipped; use `http://watch.local`. |
| **Software restart while WiFi is down** | The clock runs and WiFi is retried every 20 s. The setup network does **not** open. Unplug and replug the watch to get the power-on behaviour above. |
| **WiFi drops while running** | The clock keeps counting and WiFi is retried every 20 s. The time syncs again after reconnecting, and the web page comes back. |

Without internet, the clock drifts by roughly a few seconds per day. It is corrected at the next sync. While online, it syncs every 30 minutes.

The time is lost on a **power cut**, because the board has no battery-backed clock. Until WiFi is back, the watch shows the WiFi start-up screens instead of the clock.

## Storage format

Everything is stored in LittleFS:

| File | Contents |
|---|---|
| `/day.raw`, `/night.raw` | Pictures as RGB565: 2 bytes width, 2 bytes height, then pixel rows. Converted on the watch from the uploaded 24-bit BMP; the BMP is deleted afterwards. |
| `/schedule.cfg` | 4 bytes: day hour, day minute, night hour, night minute. Default 06:00 / 19:00. |
| `/labelpos.cfg` | Clock X and Y, 2 bytes each. |
| `/clock.cfg` | 4 bytes: colour (RGB565, low byte first), font index, show-time mode. |

## Customising

| What | Where |
|---|---|
| Firmware version | `FW_VERSION` in `Day_Night_Watch.ino` |
| Host name (`watch.local`) | `HOSTNAME` in `Day_Night_Watch.ino` |
| Time zone | `TZ_RULE` in `Day_Night_Watch.ino`, as a POSIX TZ string |
| Fallback colours | `DAY_FALLBACK_COLOR` / `NIGHT_FALLBACK_COLOR` |
| WiFi retry and setup timeouts | `WIFI_RETRY_MS`, `WIFI_SETUP_TIMEOUT_S`, `WIFI_REBEGIN_MS` |
| Clock fonts | `CLOCK_FONTS[]`. Add new fonts **at the end**, because the saved setting is the list position. |

## Project files

| File | Contents |
|---|---|
| `Day_Night_Watch.ino` | Setup, WiFi start-up, timekeeping, main loop and drawing |
| `WebUpload.ino` | Web page (HTML/JS), image upload, settings, firmware update and device endpoints |
| `Config.ino` | Loading and saving the schedule, clock position and clock settings |
| `graphics.h` | Text labels, clock text, background buffer, BMP→RAW conversion and picture drawing |
| `partitions.csv` | Custom flash layout: two 1.5 MB app slots and 896 KB of storage |
| `secrets.h` | Firmware update password (not in git) |

## Troubleshooting

| Symptom | Cause / fix |
|---|---|
| Board reboots in a loop, serial shows `TG1WDT_SYS_RST` | Wrong or extra TFT_eSPI setup active. See [Setup step 1](#1-configure-tft_espi-for-this-display). |
| `Update failed: Partition Could Not be Found` | The board was flashed without the custom partition table. Set Partition Scheme to **Custom** and upload over USB. |
| "Sketch too big" in the IDE | Partition Scheme isn't set to **Custom**. |
| `watch.local` doesn't open | Your device doesn't support mDNS (some Android versions). Use the IP address from the boot screen or your router. |
| Screen asks to join the setup network after every boot | No WiFi is saved, or the saved one is unreachable. Configure it through the setup network. |
| "Storage error" on boot | LittleFS couldn't mount. Re-upload over USB with Partition Scheme **Custom**. |

## License

See [LICENSE](LICENSE).
