#pragma once

// WiFi credentials and Tapo settings are injected via build_flags from
// environment variables (WIFI_SSID, WIFI_PASSWORD, TAPO_IP, TAPO_EMAIL, TAPO_PASSWORD).
// See platformio.ini and README for setup instructions.

// Timing
#define COUNTDOWN_SECONDS      180    // Auto-off countdown (seconds)
#define COUNTDOWN_REFRESH_MS   30000  // Refresh interval (ms)
#define WIFI_CONNECT_TIMEOUT   3000  // WiFi timeout (ms)
#define HANDSHAKE_RETRY_DELAY  1000   // Retry delay (ms)
#define MAX_RETRIES            2      // Max retry count

#ifdef ESP8266
  // ESP8266MOD — NeoPixel LED なし
  #define BUTTON_PIN        5
#else
  // M5Stack Atom Lite (ESP32)
  #define NEOPIXEL_PIN       27
  #define NEOPIXEL_NUM       1
  #define NEOPIXEL_BRIGHTNESS 10
  #define BUTTON_PIN         39
#endif
