#include "LittleFS.h"
#include "M5Dial.h"

//---COLOR PALETTE
#define COLOR_GREEN_MAIN    0x07E0   // #00FF00
#define COLOR_GREEN_NEON     0x37E2   // #39FF14
#define COLOR_GREEN_MUTED    0x2D4B   // #2E8B57
#define COLOR_BG_BLACK        0x0000   // #000000
#define COLOR_WHITE            0xFFFF   // #FFFFFF
#define COLOR_AMBER_ALERT     0xFD80   // #FFB000

//---CONFIG
#define ENCODER_STEP            4     // pulses per detent (tune if too fast/slow)
#define GESTURE_WINDOW_MS       1000   // max time span for a 3-click gesture 1sec
#define MIN_DETENT_INTERVAL_MS   6    // ignore debounce-detents faster than this (electrical noise)0.006sec
#define LOGO_FILENAME  "/Ultimatrix_Logo240x240px.jpg"

const int8_t CLOCK_PATTERN[3] = { +1, -1, +1 };  // detect wiggle gesture

// ---- alien playlists: file prefix + how many images it holds ----
// file on LittleFS = "/<prefix>_<index>.png"   e.g. "/alien_1_7.png"
struct Playlist {
  const char* prefix;
  uint8_t     count;
};

const Playlist PLAYLISTS[] = {
  { "alien_1", 20 },   // playlist 1 -> /alien_1_1.png ... /alien_1_20.png
  { "alien_2", 20 },   // playlist 2 -> /alien_2_1.png ... /alien_2_20.png
  { "alien_3", 20 },   // playlist 3 -> /alien_3_1.png ... /alien_3_20.png
  { "alien_5",  2 },   // playlist 4 -> /alien_5_1.png ... /alien_5_2.png
};
const int PLAYLIST_COUNT = sizeof(PLAYLISTS) / sizeof(PLAYLISTS[0]);

enum AppState { HOME, ALIEN_SELECT, CLOCK };
AppState state = HOME;

// ---- alien select state ----
int currentPlaylist  = 1;   // 1..PLAYLIST_COUNT
int indexInPlaylist  = 1;   // 1..PLAYLISTS[currentPlaylist - 1].count

long lastEncPos = 0;
long encAccum    = 0;
unsigned long lastDetentMillis = 0;

// ---- pending-gesture buffer, used by HOME and CLOCK to watch for the wiggle ----
int8_t        pendingBuf[3];
unsigned long pendingStart = 0;
int pendingCount = 0;

// ---- gesture buffer used only inside ALIEN_SELECT for the playlist gesture ----
int8_t        alienGestBuf[3];
unsigned long alienGestTime[3];
int alienGestCount = 0;

int lastDrawnSecond = -1;
bool logoLoaded = false;

// ================= SETUP =================
void setup() {
  auto cfg = M5.config();
  M5Dial.begin(cfg, true, false);   // (config, enable encoder, enable RFID)

  M5Dial.Display.setTextDatum(middle_center);
  M5Dial.Display.setTextFont(&fonts::Orbitron_Light_32);

  // ---- OPTIONAL: set RTC time manually once, then comment this line out ----
  // M5Dial.Rtc.setDateTime({ {2026, 7, 17}, {14, 30, 0} });

  logoLoaded = LittleFS.begin();
  if (!logoLoaded) {
    Serial.println("[LittleFS] mount FAILED - check Partition Scheme has an FS partition.");
  } else if (!LittleFS.exists(LOGO_FILENAME)) {
    Serial.println("[LittleFS] mounted OK, but " LOGO_FILENAME " was not found.");
    Serial.println("[LittleFS] Did you actually run the filesystem upload tool? (see comments in setup())");
    logoLoaded = false;
  } else {
    Serial.println("[LittleFS] logo file found, ready to draw.");
  }

  lastEncPos = M5Dial.Encoder.read();
  drawHomeScreen();
}

// ================= LOOP =================
void loop() {
  M5Dial.update();
  handleButton();
  handleEncoder();

  if (state == CLOCK) {
    updateClockScreen();
  }
}

// ================= BUTTON: only does one thing =================
void handleButton() {
  if (M5Dial.BtnA.wasPressed() && state == ALIEN_SELECT) {
    goToHome();
  }
}

void goToHome() {
  state = HOME;
  resetAllGestureTracking();
  drawHomeScreen();
}

void goToClock() {
  state = CLOCK;
  resetAllGestureTracking();
  lastDrawnSecond = -1;
  M5Dial.Speaker.tone(9000, 60);
  drawClockScreen();
}

void resetAllGestureTracking() {
  lastEncPos = M5Dial.Encoder.read();
  encAccum = 0;
  pendingCount = 0;
  alienGestCount = 0;
}

// ================= ENCODER DISPATCH =================
void handleEncoder() {
  long pos  = M5Dial.Encoder.read();
  long diff = pos - lastEncPos;
  lastEncPos = pos;
  encAccum += diff;

  while (encAccum >= ENCODER_STEP) {
    encAccum -= ENCODER_STEP;
    onDetent(+1);
  }
  while (encAccum <= -ENCODER_STEP) {
    encAccum += ENCODER_STEP;
    onDetent(-1);
  }
}

void onDetent(int8_t dir) {
  unsigned long now = millis();

  // ---- manual debounce: reject detents that arrive impossibly fast ----
  if (now - lastDetentMillis < MIN_DETENT_INTERVAL_MS) return;
  lastDetentMillis = now;

  switch (state) {
    case HOME:          handleHomeDetent(dir, now);   break;
    case ALIEN_SELECT:   handleAlienDetent(dir, now);  break;
    case CLOCK:            handleClockDetent(dir, now);  break;
  }
}

// ================= HOME: rotation enters ALIEN_SELECT, wiggle enters CLOCK =================
void handleHomeDetent(int8_t dir, unsigned long now) {
  // flush a stale pending buffer if too much time has passed
  if (pendingCount > 0 && (now - pendingStart) > GESTURE_WINDOW_MS) {
    flushPendingAsNavigation();
  }

  if (pendingCount < 3 && dir == CLOCK_PATTERN[pendingCount]) {
    // this tick still matches the wiggle pattern so far - keep waiting
    if (pendingCount == 0) pendingStart = now;
    pendingBuf[pendingCount++] = dir;

    if (pendingCount == 3) {
      pendingCount = 0;
      goToClock();   // full wiggle matched
    }
    // else: not enough ticks yet, don't reveal the alien screen yet
  } else {
    // this tick breaks the wiggle pattern - whatever was pending is real navigation
    flushPendingAsNavigation();
    // re-evaluate this same tick as a fresh possible gesture start
    if (dir == CLOCK_PATTERN[0]) {
      pendingBuf[0] = dir;
      pendingCount = 1;
      pendingStart = now;
    }
  }
}

// Whatever ticks were buffered while "waiting to see" turn out to be normal
// navigation - the first tick just reveals alien 1, any further buffered
// ticks (rare) are replayed as real steps.
void flushPendingAsNavigation() {
  if (pendingCount == 0) return;
  enterAlienSelectFromHome();
  for (int i = 1; i < pendingCount; i++) {
    applyAlienStep(pendingBuf[i]);
  }
  pendingCount = 0;
}

void enterAlienSelectFromHome() {
  state = ALIEN_SELECT;
  currentPlaylist = 1;
  indexInPlaylist = 1;
  alienGestCount = 0;
  drawAlienScreen();   // reveal immediately, no button needed
}

// ================= CLOCK: only watches for the exit wiggle =================
void handleClockDetent(int8_t dir, unsigned long now) {
  if (pendingCount > 0 && (now - pendingStart) > GESTURE_WINDOW_MS) {
    pendingCount = 0;   // stale attempt, just forget it (nothing else to do in CLOCK)
  }

  if (pendingCount < 3 && dir == CLOCK_PATTERN[pendingCount]) {
    if (pendingCount == 0) pendingStart = now;
    pendingBuf[pendingCount++] = dir;
    if (pendingCount == 3) {
      pendingCount = 0;
      goToHome();   // full wiggle matched -> exit clock
    }
  } else {
    // mismatch - restart matching from this tick if it happens to be a valid start
    pendingCount = (dir == CLOCK_PATTERN[0]) ? 1 : 0;
    if (pendingCount == 1) { pendingBuf[0] = dir; pendingStart = now; }
  }
}

// ================= ALIEN_SELECT: alien nav + playlist gesture =================
void handleAlienDetent(int8_t dir, unsigned long now) {
  // feed the playlist-gesture buffer
  for (int i = 0; i < 2; i++) {
    alienGestBuf[i]  = alienGestBuf[i + 1];
    alienGestTime[i] = alienGestTime[i + 1];
  }
  alienGestBuf[2]  = dir;
  alienGestTime[2] = now;
  if (alienGestCount < 3) alienGestCount++;

  bool withinWindow = (alienGestCount == 3) && (now - alienGestTime[0]) <= GESTURE_WINDOW_MS;

  if (withinWindow) {
    if (alienGestBuf[0] == +1 && alienGestBuf[1] == +1 && alienGestBuf[2] == -1) {
      changePlaylist(+1);
      alienGestCount = 0;
      return;
    }
    if (alienGestBuf[0] == -1 && alienGestBuf[1] == -1 && alienGestBuf[2] == +1) {
      changePlaylist(-1);
      alienGestCount = 0;
      return;
    }
  }

  applyAlienStep(dir);
}

void applyAlienStep(int8_t dir) {
  if (dir > 0 && indexInPlaylist < PLAYLISTS[currentPlaylist - 1].count) {
    indexInPlaylist++;
    M5Dial.Speaker.tone(6000, 15);
    drawAlienScreen();
  } else if (dir < 0 && indexInPlaylist > 1) {
    indexInPlaylist--;
    M5Dial.Speaker.tone(4000, 15);
    drawAlienScreen();
  }
}

void changePlaylist(int delta) {
  int newPl = currentPlaylist + delta;
  if (newPl < 1) newPl = 1;
  if (newPl > PLAYLIST_COUNT) newPl = PLAYLIST_COUNT;
  if (newPl != currentPlaylist) {
    currentPlaylist = newPl;
    indexInPlaylist = 1;
    M5Dial.Speaker.tone(8000, 40);
    drawAlienScreen();
  }
}

// ================= SCREENS =================

void drawHomeScreen() {
  M5Dial.Display.fillScreen(COLOR_BG_BLACK);
  if (logoLoaded) {
    M5Dial.Display.drawJpgFile(LittleFS, LOGO_FILENAME, 0, 0);
  } else {
    drawOmnitrixLogoVector(120, 120, 70);
  }
}

void drawAlienScreen() {
  char path[32];
  snprintf(path, sizeof(path), "/%s_%d.png", PLAYLISTS[currentPlaylist - 1].prefix, indexInPlaylist);

  M5Dial.Display.fillScreen(COLOR_BG_BLACK);   // black behind any transparent PNG pixels

  if (LittleFS.exists(path)) {
    M5Dial.Display.drawPngFile(LittleFS, path, 0, 0);
  } else {
    // fallback: show which file is missing instead of a blank screen
    Serial.printf("[LittleFS] missing %s\n", path);
    M5Dial.Display.setTextColor(COLOR_AMBER_ALERT);
    M5Dial.Display.setTextSize(0.5);
    M5Dial.Display.drawString(path, 120, 120);
  }
}

void drawOmnitrixLogoVector(int cx, int cy, int r) {
  M5Dial.Display.fillCircle(cx, cy, r, COLOR_GREEN_MAIN);
  M5Dial.Display.fillCircle(cx, cy, r - 10, COLOR_BG_BLACK);
  M5Dial.Display.fillTriangle(cx - 30, cy - 30, cx + 30, cy - 30, cx, cy, COLOR_GREEN_MAIN);
  M5Dial.Display.fillTriangle(cx - 30, cy + 30, cx + 30, cy + 30, cx, cy, COLOR_GREEN_MAIN);
}

void updateClockScreen() {
  auto dt = M5Dial.Rtc.getDateTime();
  if (dt.time.seconds == lastDrawnSecond) return;
  lastDrawnSecond = dt.time.seconds;
  drawClockScreen();
}

void drawClockScreen() {
  auto dt = M5Dial.Rtc.getDateTime();
  static constexpr const char* const wd[7] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};

  M5Dial.Display.fillScreen(COLOR_BG_BLACK);

  char timeBuf[9];
  sprintf(timeBuf, "%02d:%02d:%02d", dt.time.hours, dt.time.minutes, dt.time.seconds);
  M5Dial.Display.setTextColor(COLOR_AMBER_ALERT);
  M5Dial.Display.setTextSize(2);
  M5Dial.Display.drawString(timeBuf, 120, 110);

  char dateBuf[16];
  sprintf(dateBuf, "%04d-%02d-%02d", dt.date.year, dt.date.month, dt.date.date);
  M5Dial.Display.setTextSize(1);
  M5Dial.Display.setTextColor(COLOR_GREEN_MUTED);
  M5Dial.Display.drawString(String(dateBuf) + " (" + wd[dt.date.weekDay] + ")", 120, 150);
}