#pragma once

#ifdef ESP8266
// ESP8266MOD は NeoPixel LED なし — 全メソッドを no-op スタブとして定義
#include <Arduino.h>

class LedStatus {
public:
    void begin() {}
    void setSolid(uint8_t, uint8_t, uint8_t) {}
    void setBlink(uint8_t, uint8_t, uint8_t, uint16_t) {}
    void off() {}
    void update() {}
    void wifiConnecting() {}
    void handshaking() {}
    void ok() {}
    void error() {}
    void refreshing() {}
};

#else
// M5Stack Atom Lite (ESP32)
#include <Arduino.h>
#include <FastLED.h>
#include "config.h"

class LedStatus {
public:
    void begin();
    void setSolid(uint8_t r, uint8_t g, uint8_t b);
    void setBlink(uint8_t r, uint8_t g, uint8_t b, uint16_t intervalMs);
    void off();
    void update(); // Call in loop() for non-blocking blink

    // Convenience methods
    void wifiConnecting()  { setBlink(0, 0, 255, 500); }   // Blue blink
    void handshaking()     { setSolid(255, 200, 0); }       // Yellow solid
    void ok()              { setSolid(0, 255, 0); }         // Green solid
    void error()           { setBlink(255, 0, 0, 200); }    // Red fast blink
    void refreshing()      { setSolid(255, 255, 255); }     // White flash

private:
    CRGB led_[NEOPIXEL_NUM];
    bool blinkActive_ = false;
    uint16_t blinkInterval_ = 0;
    CRGB blinkColor_;
    bool ledOn_ = false;
    uint32_t lastToggle_ = 0;
};

#endif
