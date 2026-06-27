#include "wifi_manager.h"
#include <WiFi.h>
#include <ESPmDNS.h>

static const char* TAG = "wifi";

void WifiManager::begin(const String& ssid, const String& password, const String& hostname) {
    _ssid = ssid;
    _password = password;
    _state = WifiState::CONNECTING;

    WiFi.mode(WIFI_STA);
    WiFi.setHostname(hostname.c_str());
    WiFi.setAutoReconnect(true);
    WiFi.begin(ssid.c_str(), password.c_str());

    log_i("Connecting to WiFi: %s", ssid.c_str());

    // Block for initial connection (up to 10s)
    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
        delay(100);
    }

    if (WiFi.status() == WL_CONNECTED) {
        _state = WifiState::CONNECTED;
        _retryCount = 0;
        log_i("WiFi connected: %s (RSSI: %d)", WiFi.localIP().toString().c_str(), WiFi.RSSI());

        if (MDNS.begin(hostname.c_str())) {
            log_i("mDNS: %s.local", hostname.c_str());
        }
    } else {
        _state = WifiState::DISCONNECTED;
        log_w("WiFi initial connection failed, will retry");
    }
}

void WifiManager::beginAP(const String& apName) {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(apName.c_str());
    _state = WifiState::AP_MODE;
    log_i("AP mode started: %s (IP: %s)", apName.c_str(), WiFi.softAPIP().toString().c_str());
}

void WifiManager::loop() {
    if (_state == WifiState::AP_MODE) return;

    if (WiFi.status() == WL_CONNECTED) {
        if (_state != WifiState::CONNECTED) {
            _state = WifiState::CONNECTED;
            _retryCount = 0;
            log_i("WiFi reconnected: %s", WiFi.localIP().toString().c_str());
        }
        return;
    }

    // Disconnected — attempt reconnect with backoff
    _state = WifiState::DISCONNECTED;
    uint32_t now = millis();
    if (now - _lastAttempt < _reconnectInterval) return;

    _lastAttempt = now;
    _retryCount++;
    _reconnectInterval = min((uint32_t)60000, _reconnectInterval * 2);

    log_i("WiFi reconnect attempt %d...", _retryCount);
    WiFi.disconnect();
    WiFi.begin(_ssid.c_str(), _password.c_str());
}

bool WifiManager::isConnected() const {
    return WiFi.status() == WL_CONNECTED;
}

String WifiManager::localIP() const {
    if (_state == WifiState::AP_MODE) {
        return WiFi.softAPIP().toString();
    }
    return WiFi.localIP().toString();
}

int32_t WifiManager::rssi() const {
    return WiFi.RSSI();
}
