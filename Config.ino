// ---- Day/Night schedule: persistent storage and time comparison ----
// Stored as 4 bytes in /schedule.cfg: [dayHour, dayMin, nightHour, nightMin]
// Defaults: day starts 06:00, night starts 19:00

struct Schedule {
  uint8_t dayHour   = 6;
  uint8_t dayMin    = 0;
  uint8_t nightHour = 19;
  uint8_t nightMin  = 0;
};

Schedule schedule;

void loadSchedule() {
  File f = LittleFS.open("/schedule.cfg", "r");
  if (!f || f.size() < 4) {
    schedule = {6, 0, 19, 0};
    if (f) f.close();
    return;
  }
  uint8_t buf[4];
  f.read(buf, 4);
  f.close();
  schedule.dayHour   = constrain(buf[0], 0, 23);
  schedule.dayMin    = constrain(buf[1], 0, 59);
  schedule.nightHour = constrain(buf[2], 0, 23);
  schedule.nightMin  = constrain(buf[3], 0, 59);
}

void saveSchedule() {
  File f = LittleFS.open("/schedule.cfg", "w");
  if (!f) return;
  f.write(schedule.dayHour);
  f.write(schedule.dayMin);
  f.write(schedule.nightHour);
  f.write(schedule.nightMin);
  f.close();
}

// Returns true if the given hour:minute falls within the day window
// [day start, night start). Either window may wrap past midnight.
// If both start times are equal, it is always day.
bool isDayTime(uint8_t hour, uint8_t minute) {
  uint16_t now   = hour   * 60 + minute;
  uint16_t day   = schedule.dayHour   * 60 + schedule.dayMin;
  uint16_t night = schedule.nightHour * 60 + schedule.nightMin;

  if (day < night) {
    // Normal case: day 07:00, night 19:00 — night wraps past midnight
    return now >= day && now < night;
  } else {
    // Inverted case: day 22:00, night 06:00 — day wraps past midnight
    return now >= day || now < night;
  }
}

// ---- Label position: persistent storage ----
// labelX / labelY are defined in Day_Night_Watch.ino.

void loadLabelPos() {
  File f = LittleFS.open("/labelpos.cfg", "r");
  if (!f || f.size() < 4) {
    // Defaults are already set at declaration
    return;
  }
  f.read((uint8_t*)&labelX, 2);
  f.read((uint8_t*)&labelY, 2);
  f.close();
}

void saveLabelPos() {
  File f = LittleFS.open("/labelpos.cfg", "w");
  if (!f) return;
  f.write((uint8_t*)&labelX, 2);
  f.write((uint8_t*)&labelY, 2);
  f.close();
}

// ---- Clock settings: persistent storage ----
// timeColor / clockFont / showTime are defined in Day_Night_Watch.ino.
// Stored as 4 bytes in /clock.cfg: [color lo, color hi (RGB565), font index, show-time mode]

void loadClockSettings() {
  File f = LittleFS.open("/clock.cfg", "r");
  if (!f || f.size() < 4) {
    if (f) f.close();
    // Older firmware saved only the colour, in /timecolor.cfg
    f = LittleFS.open("/timecolor.cfg", "r");
    if (f && f.size() >= 2) f.read((uint8_t*)&timeColor, 2);
    if (f) f.close();
    return;
  }
  uint8_t buf[4];
  f.read(buf, 4);
  f.close();
  timeColor = buf[0] | (buf[1] << 8);
  clockFont = buf[2] < CLOCK_FONT_COUNT ? buf[2] : 0;
  showTime  = buf[3] <= SHOW_NEVER ? buf[3] : SHOW_ALWAYS;
}

void saveClockSettings() {
  File f = LittleFS.open("/clock.cfg", "w");
  if (!f) return;
  uint8_t buf[4] = { (uint8_t)(timeColor & 0xFF), (uint8_t)(timeColor >> 8), clockFont, showTime };
  f.write(buf, 4);
  f.close();
  LittleFS.remove("/timecolor.cfg"); // superseded by /clock.cfg
}
