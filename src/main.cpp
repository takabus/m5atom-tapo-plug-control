#include <Arduino.h>
#ifdef ESP8266
  #include <ESP8266WiFi.h>
#else
  #include <WiFi.h>
#endif
#include "config.h"
#include "led_status.h"
#include "tapo_device.h"

LedStatus led;
TapoDevice plug;
uint32_t lastRefresh = 0;

// Connect to WiFi. Returns true on success.
bool connectWiFi() {
    Serial.printf("[WiFi] Connecting to %s\n", WIFI_SSID);
    led.wifiConnecting();

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        led.update();
        delay(100);
        if (millis() - start > WIFI_CONNECT_TIMEOUT) {
            Serial.println("[WiFi] Connection timeout");
            return false;
        }
    }

    Serial.printf("[WiFi] Connected: %s\n", WiFi.localIP().toString().c_str());
    return true;
}

// Connect to Tapo device with retries. Returns true on success.
bool connectPlug() {
    led.handshaking();

    for (int i = 0; i < MAX_RETRIES; i++) {
        Serial.printf("[Main] Handshake attempt %d/%d\n", i + 1, MAX_RETRIES);
        if (plug.connect(TAPO_IP, TAPO_EMAIL, TAPO_PASSWORD)) {
            return true;
        }
        delay(HANDSHAKE_RETRY_DELAY);
    }

    Serial.println("[Main] All handshake attempts failed");
    return false;
}

// Ensure WiFi is connected, reconnect if needed.
bool ensureWiFi() {
    if (WiFi.status() == WL_CONNECTED) return true;
    Serial.println("[WiFi] Disconnected, reconnecting...");
    WiFi.disconnect();
    return connectWiFi();
}

// Refresh countdown timer with retry and reconnect logic.
bool refreshCountdown() {
    // Try sending directly
    for (int i = 0; i < 3; i++) {
        if (plug.setCountdownOff(COUNTDOWN_SECONDS)) return true;
        Serial.printf("[Main] Countdown retry %d/3\n", i + 1);
        delay(1000);
    }

    // Session may have expired, reconnect
    Serial.println("[Main] Reconnecting to plug...");
    if (plug.reconnect()) {
        if (plug.setCountdownOff(COUNTDOWN_SECONDS)) return true;
    }

    return false;
}

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=== M5Atom YHT Controller ===");

    led.begin();

    // Step 1: Connect WiFi
    if (!connectWiFi()) {
        led.error();
        delay(3000);
        ESP.restart();
    }

    // Step 2: Connect to P105
    if (!connectPlug()) {
        led.error();
        delay(3000);
        ESP.restart();
    }

    // Step 3: Turn ON plug
    bool onOk = false;
    for (int i = 0; i < 3; i++) {
        if (plug.turnOn()) { onOk = true; break; }
        delay(1000);
    }
    if (!onOk) {
        // Try reconnect once more
        if (plug.reconnect()) plug.turnOn();
    }

    // Step 4: Set initial countdown timer
    plug.setCountdownOff(COUNTDOWN_SECONDS);

    // Ready
    led.ok();
    lastRefresh = millis();
    Serial.println("[Main] Running. Countdown refresh active.");
}

void loop() {
    led.update();

    // Periodic countdown refresh
    if (millis() - lastRefresh >= COUNTDOWN_REFRESH_MS) {
        // Ensure WiFi
        if (!ensureWiFi()) {
            led.error();
            delay(5000);
            ESP.restart();
        }

        led.refreshing();
        if (refreshCountdown()) {
            led.ok();
            lastRefresh = millis();
        } else {
            Serial.println("[Main] Countdown refresh failed, restarting...");
            led.error();
            delay(3000);
            ESP.restart();
        }
    }

    delay(100);
}
