#pragma once

#include <Arduino.h>
#include "tapo_klap.h"

class TapoDevice {
public:
    TapoDevice();

    // Connect to device (performs KLAP handshake)
    bool connect(const char* ip, const char* email, const char* password);

    // Reconnect (re-handshake with stored credentials)
    bool reconnect();

    // Turn plug ON
    bool turnOn();

    // Turn plug OFF
    bool turnOff();

    // Set countdown timer to auto-off after delay_seconds
    bool setCountdownOff(uint32_t delaySec);

    // Check connection status
    bool isConnected() const { return klap_.isConnected(); }

private:
    TapoKlap klap_;
    const char* ip_;
    const char* email_;
    const char* password_;

    bool sendCommand(const String& method, const String& params);
};
