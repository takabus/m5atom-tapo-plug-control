#include "led_status.h"

void LedStatus::begin() {
    FastLED.addLeds<WS2812, NEOPIXEL_PIN, GRB>(led_, NEOPIXEL_NUM);
    FastLED.setBrightness(NEOPIXEL_BRIGHTNESS);
    off();
}

void LedStatus::setSolid(uint8_t r, uint8_t g, uint8_t b) {
    blinkActive_ = false;
    led_[0] = CRGB(r, g, b);
    FastLED.show();
}

void LedStatus::setBlink(uint8_t r, uint8_t g, uint8_t b, uint16_t intervalMs) {
    blinkActive_ = true;
    blinkInterval_ = intervalMs;
    blinkColor_ = CRGB(r, g, b);
    ledOn_ = true;
    lastToggle_ = millis();
    led_[0] = blinkColor_;
    FastLED.show();
}

void LedStatus::off() {
    blinkActive_ = false;
    led_[0] = CRGB::Black;
    FastLED.show();
}

void LedStatus::update() {
    if (!blinkActive_) return;
    uint32_t now = millis();
    if (now - lastToggle_ >= blinkInterval_) {
        lastToggle_ = now;
        ledOn_ = !ledOn_;
        led_[0] = ledOn_ ? blinkColor_ : CRGB::Black;
        FastLED.show();
    }
}
