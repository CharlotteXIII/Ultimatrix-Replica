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
#define LOGO_FILENAME  "/Ultimatrix_Logo240x240px.jpg"

void setup() {
    auto cfg = M5.config();
    M5Dial.begin(cfg, true, false);
    M5Dial.Display.setTextColor(GREEN);
    M5Dial.Display.setTextDatum(middle_center);
    M5Dial.Display.setTextFont(&fonts::Orbitron_Light_32);
    M5Dial.Display.setTextSize(2);
    Serial.begin(115200);
}

long oldPosition = -999;

void loop() {
    M5Dial.update();
    long newPosition = M5Dial.Encoder.read();
    if (newPosition != oldPosition) {
        Serial.println(oldPosition);
        M5Dial.Speaker.tone(8000, 20);
        M5Dial.Display.clear();
        oldPosition = newPosition;
        M5Dial.Display.drawString(String(newPosition),
                                  M5Dial.Display.width() / 2,
                                  M5Dial.Display.height() / 2);
    }
    if (M5Dial.BtnA.wasPressed()) {
        M5Dial.Encoder.readAndReset();
    }
}