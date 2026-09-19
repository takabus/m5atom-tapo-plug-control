#pragma once

// WiFi credentials and Tapo settings (WIFI_SSID, WIFI_PASSWORD, TAPO_IP,
// TAPO_EMAIL, TAPO_PASSWORD) are injected at build time by
// scripts/load_env.py, which reads the project-root .env file.
// Copy .env.example to .env and fill it in. Real environment variables
// take precedence over .env.

// Timing
#define COUNTDOWN_SECONDS      180    // Auto-off countdown (seconds)
#define COUNTDOWN_REFRESH_MS   30000  // Refresh interval (ms)
#define WIFI_CONNECT_TIMEOUT   5000  // WiFi timeout (ms)
#define HANDSHAKE_RETRY_DELAY  3000   // Retry delay (ms)
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
