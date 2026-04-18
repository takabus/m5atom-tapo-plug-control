#pragma once

#include <Arduino.h>
#include "tapo_cipher.h"

class TapoKlap {
public:
    TapoKlap();
    ~TapoKlap();

    // Perform full KLAP handshake. Returns true on success.
    bool handshake(const char* ip, const char* email, const char* password);

    // Send encrypted JSON command, receive decrypted JSON response.
    // Returns true on success, response stored in 'response'.
    bool send(const String& json, String& response);

    // Check if session is established
    bool isConnected() const { return connected_; }

    // Get last HTTP status code
    int getLastHttpCode() const { return lastHttpCode_; }

private:
    String baseUrl_;
    String cookie_;
    TapoCipher cipher_;
    uint8_t authHash_[32];
    bool connected_;
    int lastHttpCode_;

    bool handshake1(const uint8_t localSeed[16], uint8_t remoteSeed[16]);
    bool handshake2(const uint8_t localSeed[16], const uint8_t remoteSeed[16]);
    String extractCookie(const String& setCookie);
};
