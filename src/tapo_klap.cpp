#include "tapo_klap.h"
#include <HTTPClient.h>
#include <WiFi.h>
#include <esp_random.h>

TapoKlap::TapoKlap() : connected_(false), lastHttpCode_(0) {
    memset(authHash_, 0, 32);
}

TapoKlap::~TapoKlap() {}

String TapoKlap::extractCookie(const String& setCookie) {
    // Extract TP_SESSIONID from Set-Cookie header
    int start = setCookie.indexOf("TP_SESSIONID=");
    if (start < 0) return "";
    start += strlen("TP_SESSIONID=");
    int end = setCookie.indexOf(";", start);
    if (end < 0) end = setCookie.length();
    return "TP_SESSIONID=" + setCookie.substring(start, end);
}

bool TapoKlap::handshake(const char* ip, const char* email, const char* password) {
    connected_ = false;
    baseUrl_ = "http://" + String(ip) + "/app";
    cookie_ = "";

    // Compute auth hash
    TapoCipher::computeAuthHash(email, password, authHash_);

    // Generate 16-byte random local seed
    uint8_t localSeed[16];
    esp_fill_random(localSeed, 16);

    uint8_t remoteSeed[16];

    // Handshake step 1
    if (!handshake1(localSeed, remoteSeed)) {
        Serial.println("[KLAP] Handshake1 failed");
        return false;
    }

    // Handshake step 2
    if (!handshake2(localSeed, remoteSeed)) {
        Serial.println("[KLAP] Handshake2 failed");
        return false;
    }

    // Derive encryption keys
    cipher_.deriveKeys(localSeed, remoteSeed, authHash_);

    connected_ = true;
    Serial.println("[KLAP] Handshake complete");
    return true;
}

bool TapoKlap::handshake1(const uint8_t localSeed[16], uint8_t remoteSeed[16]) {
    HTTPClient http;
    http.begin(baseUrl_ + "/handshake1");
    http.addHeader("Content-Type", "application/octet-stream");
    http.setTimeout(5000);

    // HTTPClient does not retain response headers unless explicitly requested
    const char* headerKeys[] = {"Set-Cookie"};
    http.collectHeaders(headerKeys, 1);

    int httpCode = http.POST((uint8_t*)localSeed, 16);
    lastHttpCode_ = httpCode;

    if (httpCode != 200) {
        Serial.printf("[KLAP] handshake1 HTTP %d\n", httpCode);
        http.end();
        return false;
    }

    // Response: remote_seed(16) + server_hash(32) = 48 bytes
    int responseLen = http.getSize();
    if (responseLen < 48) {
        Serial.printf("[KLAP] handshake1 response too short: %d\n", responseLen);
        http.end();
        return false;
    }

    uint8_t response[48];
    WiFiClient* stream = http.getStreamPtr();
    size_t bytesRead = 0;
    unsigned long deadline = millis() + 3000;
    while (bytesRead < 48 && millis() < deadline) {
        if (stream->available()) {
            int b = stream->read();
            if (b >= 0) response[bytesRead++] = (uint8_t)b;
        }
    }

    if (bytesRead < 48) {
        Serial.printf("[KLAP] handshake1 read only %d bytes\n", (int)bytesRead);
        http.end();
        return false;
    }

    memcpy(remoteSeed, response, 16);
    uint8_t serverHash[32];
    memcpy(serverHash, response + 16, 32);

    // Verify server_hash = SHA256(local_seed + remote_seed + auth_hash)
    uint8_t expectedHash[32];
    const uint8_t* parts[] = {localSeed, remoteSeed, authHash_};
    const size_t lengths[] = {16, 16, 32};
    TapoCipher::sha256_multi(parts, lengths, 3, expectedHash);

    if (memcmp(serverHash, expectedHash, 32) != 0) {
        Serial.println("[KLAP] handshake1 server hash mismatch (auth error)");
        http.end();
        return false;
    }

    // Extract cookie
    String setCookie = http.header("Set-Cookie");
    if (setCookie.length() > 0) {
        cookie_ = extractCookie(setCookie);
        Serial.printf("[KLAP] Cookie: %s\n", cookie_.c_str());
    }

    http.end();
    return true;
}

bool TapoKlap::handshake2(const uint8_t localSeed[16], const uint8_t remoteSeed[16]) {
    // client_hash = SHA256(remote_seed + local_seed + auth_hash)
    uint8_t clientHash[32];
    const uint8_t* parts[] = {remoteSeed, localSeed, authHash_};
    const size_t lengths[] = {16, 16, 32};
    TapoCipher::sha256_multi(parts, lengths, 3, clientHash);

    HTTPClient http;
    http.begin(baseUrl_ + "/handshake2");
    http.addHeader("Content-Type", "application/octet-stream");
    if (cookie_.length() > 0) {
        http.addHeader("Cookie", cookie_);
    }
    http.setTimeout(5000);

    int httpCode = http.POST(clientHash, 32);
    lastHttpCode_ = httpCode;
    http.end();

    if (httpCode != 200) {
        Serial.printf("[KLAP] handshake2 HTTP %d\n", httpCode);
        return false;
    }

    Serial.println("[KLAP] Handshake2 OK");
    return true;
}

bool TapoKlap::send(const String& json, String& response) {
    if (!connected_) return false;

    // Encrypt
    std::vector<uint8_t> payload = cipher_.encrypt(json.c_str());
    if (payload.empty()) return false;

    // Build URL with seq parameter
    String url = baseUrl_ + "/request?seq=" + String(cipher_.getSeq());

    HTTPClient http;
    http.begin(url);
    http.addHeader("Content-Type", "application/octet-stream");
    if (cookie_.length() > 0) {
        http.addHeader("Cookie", cookie_);
    }
    http.setTimeout(5000);

    int httpCode = http.POST(payload.data(), payload.size());
    lastHttpCode_ = httpCode;

    if (httpCode != 200) {
        Serial.printf("[KLAP] request HTTP %d\n", httpCode);
        if (httpCode == 403) {
            connected_ = false; // Session expired
        }
        http.end();
        return false;
    }

    // Read encrypted response
    int respLen = http.getSize();
    if (respLen <= 0) {
        // Try reading available data
        String raw = http.getString();
        http.end();
        if (raw.length() == 0) return false;
        // Decrypt
        std::vector<uint8_t> decrypted = cipher_.decrypt((const uint8_t*)raw.c_str(), raw.length());
        if (decrypted.empty()) return false;
        response = String((const char*)decrypted.data(), decrypted.size());
        return true;
    }

    std::vector<uint8_t> respData(respLen);
    WiFiClient* stream = http.getStreamPtr();
    size_t bytesRead = 0;
    unsigned long startTime = millis();
    while (bytesRead < (size_t)respLen && (millis() - startTime < 5000)) {
        if (stream->available()) {
            int b = stream->read();
            if (b >= 0) respData[bytesRead++] = (uint8_t)b;
        }
    }
    http.end();

    if (bytesRead < (size_t)respLen) return false;

    // Decrypt response
    std::vector<uint8_t> decrypted = cipher_.decrypt(respData.data(), bytesRead);
    if (decrypted.empty()) return false;

    response = String((const char*)decrypted.data(), decrypted.size());
    return true;
}
