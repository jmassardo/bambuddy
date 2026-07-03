#include "api_client.h"
#include "config.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

void ApiClient::begin(const char* ssid, const char* password) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start) < WIFI_CONNECT_TIMEOUT) {
        delay(250);
    }
    _wifiConnected = (WiFi.status() == WL_CONNECTED);
    if (_wifiConnected) {
        _rssi = WiFi.RSSI();
    }
}

void ApiClient::loop() {
    uint32_t now = millis();

    // Check WiFi periodically
    if (now - _lastWifiCheck > 5000) {
        _lastWifiCheck = now;
        _wifiConnected = (WiFi.status() == WL_CONNECTED);
        if (_wifiConnected) {
            _rssi = WiFi.RSSI();
        } else {
            _rssi = -100;
            _serverConnected = false;
            WiFi.reconnect();
        }
    }

    if (!_wifiConnected) return;

    // Heartbeat
    if (now - _lastHeartbeat > HEARTBEAT_INTERVAL) {
        _lastHeartbeat = now;
        _serverConnected = sendHeartbeat();
    }
}

void ApiClient::setServer(const char* host, uint16_t port, const char* apiKey) {
    strncpy(_serverHost, host, sizeof(_serverHost) - 1);
    _serverPort = port;
    strncpy(_apiKey, apiKey, sizeof(_apiKey) - 1);
}

void ApiClient::setDeviceId(const char* deviceId) {
    strncpy(_deviceId, deviceId, sizeof(_deviceId) - 1);
}

bool ApiClient::registerDevice() {
    JsonDocument doc;
    doc["device_id"] = _deviceId;
    doc["device_type"] = DEVICE_TYPE;
    doc["firmware_version"] = FIRMWARE_VERSION;
    if (_printerId >= 0) doc["printer_id"] = _printerId;

    String body;
    serializeJson(doc, body);

    String url = _buildUrl("/api/v1/spoolbuddy/register");
    String response;
    return _httpPost(url.c_str(), body.c_str(), response);
}

bool ApiClient::sendHeartbeat() {
    JsonDocument doc;
    doc["firmware_version"] = FIRMWARE_VERSION;
    doc["wifi_rssi"] = _rssi;
    doc["device_type"] = DEVICE_TYPE;

    String body;
    serializeJson(doc, body);

    String url = _buildUrl(String("/api/v1/spoolbuddy/" + String(_deviceId) + "/heartbeat").c_str());
    String response;
    if (!_httpPost(url.c_str(), body.c_str(), response)) return false;

    // Parse heartbeat response for config updates / OTA
    JsonDocument respDoc;
    if (deserializeJson(respDoc, response) == DeserializationError::Ok) {
        // Check for OTA
        if (respDoc["ota_url"].is<const char*>()) {
            strncpy(_otaUrl, respDoc["ota_url"].as<const char*>(), sizeof(_otaUrl) - 1);
        }
        // Check for config update (printer_id change)
        if (respDoc["config_update"].is<JsonObject>()) {
            JsonObject cfg = respDoc["config_update"].as<JsonObject>();
            if (cfg["printer_id"].is<int>()) {
                _printerId = cfg["printer_id"].as<int>();
            }
        }
    }

    return true;
}

bool ApiClient::fetchPrinterStatus(PrinterStatus& out, HmsError* hmsOut, uint8_t& hmsCount) {
    String url = _buildUrl(String("/api/v1/spoolbuddy/devices/" + String(_deviceId) + "/printer-status").c_str());
    String response;
    if (!_httpGet(url.c_str(), response)) return false;

    JsonDocument doc;
    if (deserializeJson(doc, response) != DeserializationError::Ok) return false;

    strncpy(out.name, doc["name"] | "???", sizeof(out.name) - 1);
    strncpy(out.state, doc["state"] | "unknown", sizeof(out.state) - 1);
    out.progress = doc["progress"] | 0.0f;
    out.timeRemaining = doc["time_remaining_min"] | 0;
    strncpy(out.jobName, doc["job_name"] | "", sizeof(out.jobName) - 1);
    out.awaitingClear = doc["awaiting_plate_clear"] | false;

    // HMS errors
    hmsCount = 0;
    if (doc["hms_errors"].is<JsonArray>()) {
        JsonArray arr = doc["hms_errors"].as<JsonArray>();
        for (JsonObject e : arr) {
            if (hmsCount >= 8) break;
            strncpy(hmsOut[hmsCount].code, e["code"] | "", sizeof(hmsOut[0].code) - 1);
            strncpy(hmsOut[hmsCount].shortDesc, e["short"] | "", sizeof(hmsOut[0].shortDesc) - 1);
            hmsOut[hmsCount].severity = e["severity"] | 4;
            hmsCount++;
        }
    }

    return true;
}

bool ApiClient::clearPlate() {
    String url = _buildUrl(String("/api/v1/spoolbuddy/devices/" + String(_deviceId) + "/plate-clear").c_str());
    String response;
    return _httpPost(url.c_str(), "{}", response);
}

bool ApiClient::fetchQueue(char jobs[][64], uint8_t& count) {
    String url = _buildUrl(String("/api/v1/spoolbuddy/devices/" + String(_deviceId) + "/queue").c_str());
    String response;
    if (!_httpGet(url.c_str(), response)) return false;

    JsonDocument doc;
    if (deserializeJson(doc, response) != DeserializationError::Ok) return false;

    count = 0;
    if (doc["jobs"].is<JsonArray>()) {
        JsonArray arr = doc["jobs"].as<JsonArray>();
        for (JsonObject job : arr) {
            if (count >= 5) break;
            strncpy(jobs[count], job["name"] | "???", 63);
            jobs[count][63] = '\0';
            count++;
        }
    }
    return true;
}

bool ApiClient::ackHms() {
    String url = _buildUrl(String("/api/v1/spoolbuddy/devices/" + String(_deviceId) + "/hms-ack").c_str());
    String response;
    return _httpPost(url.c_str(), "{}", response);
}

// --- Helpers ---

String ApiClient::_buildUrl(const char* path) {
    return String("http://") + _serverHost + ":" + String(_serverPort) + path;
}

bool ApiClient::_httpGet(const char* url, String& response) {
    HTTPClient http;
    http.begin(url);
    http.setTimeout(HTTP_TIMEOUT);
    http.addHeader("X-API-Key", _apiKey);

    int code = http.GET();
    if (code == 200) {
        response = http.getString();
        http.end();
        return true;
    }
    http.end();
    return false;
}

bool ApiClient::_httpPost(const char* url, const char* body, String& response) {
    HTTPClient http;
    http.begin(url);
    http.setTimeout(HTTP_TIMEOUT);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-API-Key", _apiKey);

    int code = http.POST(body);
    if (code >= 200 && code < 300) {
        response = http.getString();
        http.end();
        return true;
    }
    http.end();
    return false;
}
