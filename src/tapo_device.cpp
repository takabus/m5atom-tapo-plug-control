#include "tapo_device.h"
#include <ArduinoJson.h>

TapoDevice::TapoDevice() : ip_(nullptr), email_(nullptr), password_(nullptr) {}

bool TapoDevice::connect(const char* ip, const char* email, const char* password) {
    ip_ = ip;
    email_ = email;
    password_ = password;
    return klap_.handshake(ip, email, password);
}

bool TapoDevice::reconnect() {
    if (!ip_ || !email_ || !password_) return false;
    Serial.println("[Device] Reconnecting...");
    return klap_.handshake(ip_, email_, password_);
}

bool TapoDevice::sendCommand(const String& method, const String& params) {
    // Build JSON request
    JsonDocument doc;
    doc["method"] = method;
    doc["requestTimeMils"] = (long)millis();
    doc["terminalUUID"] = "m5atom-yht";

    // Parse params JSON string into the document
    if (params.length() > 0) {
        JsonDocument paramsDoc;
        DeserializationError err = deserializeJson(paramsDoc, params);
        if (err) {
            Serial.printf("[Device] params parse error: %s\n", err.c_str());
            return false;
        }
        doc["params"] = paramsDoc.as<JsonVariant>();
    }

    String json;
    serializeJson(doc, json);
    Serial.printf("[Device] >> %s\n", json.c_str());

    String response;
    bool ok = klap_.send(json, response);
    if (!ok) {
        Serial.printf("[Device] send failed (HTTP %d)\n", klap_.getLastHttpCode());
        return false;
    }

    Serial.printf("[Device] << %s\n", response.c_str());

    // Check error_code in response
    JsonDocument respDoc;
    DeserializationError err = deserializeJson(respDoc, response);
    if (!err) {
        int errorCode = respDoc["error_code"] | -1;
        if (errorCode != 0) {
            Serial.printf("[Device] error_code: %d\n", errorCode);
            return false;
        }
    }

    return true;
}

bool TapoDevice::turnOn() {
    Serial.println("[Device] Turning ON");
    return sendCommand("set_device_info", "{\"device_on\":true}");
}

bool TapoDevice::turnOff() {
    Serial.println("[Device] Turning OFF");
    return sendCommand("set_device_info", "{\"device_on\":false}");
}

bool TapoDevice::setCountdownOff(uint32_t delaySec) {
    Serial.printf("[Device] Setting countdown: %u sec\n", delaySec);

    // Get existing countdown rules to determine whether to add or edit
    JsonDocument getDoc;
    getDoc["method"] = "get_countdown_rules";
    getDoc["requestTimeMils"] = (long)millis();
    getDoc["terminalUUID"] = "m5atom-yht";
    String getJson;
    serializeJson(getDoc, getJson);
    Serial.printf("[Device] >> %s\n", getJson.c_str());

    String existingId = "";
    String rulesResp;
    if (klap_.send(getJson, rulesResp)) {
        Serial.printf("[Device] << %s\n", rulesResp.c_str());
        JsonDocument rulesDoc;
        if (!deserializeJson(rulesDoc, rulesResp)) {
            JsonArray rules = rulesDoc["result"]["rule_list"].as<JsonArray>();
            if (rules.size() > 0) {
                const char* ruleId = rules[0]["id"];
                if (ruleId && strlen(ruleId) > 0) {
                    existingId = String(ruleId);
                }
            }
        }
    }

    // If a rule already exists, edit it in place; otherwise add a new one
    String id = existingId.length() > 0 ? existingId : "countdown_0";
    String method = existingId.length() > 0 ? "edit_countdown_rule" : "add_countdown_rule";
    String params = "{\"enable\":true,\"id\":\"" + id + "\",\"delay\":" +
                    String(delaySec) + ",\"desired_states\":{\"on\":false}}";
    return sendCommand(method, params);
}
