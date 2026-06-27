#pragma once

#include <Arduino.h>

enum class WifiState {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    AP_MODE  // Captive portal for provisioning
};

class WifiManager {
public:
    void begin(const String& ssid, const String& password, const String& hostname);
    void beginAP(const String& apName);
    void loop();

    bool isConnected() const;
    WifiState state() const { return _state; }
    String localIP() const;
    int32_t rssi() const;

private:
    WifiState _state = WifiState::DISCONNECTED;
    String _ssid;
    String _password;
    uint32_t _lastAttempt = 0;
    uint32_t _reconnectInterval = 5000;
    uint8_t _retryCount = 0;
};
