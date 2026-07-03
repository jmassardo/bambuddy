#pragma once

#include <Arduino.h>
#include "display.h"  // for PrinterStatus, HmsError

// Callback types for async responses
using StatusCallback = void (*)(const PrinterStatus& status, const HmsError* errors, uint8_t hmsCount);
using HeartbeatCallback = void (*)(bool configChanged);

class ApiClient {
public:
    void begin(const char* ssid, const char* password);
    void loop();

    bool isWifiConnected() const { return _wifiConnected; }
    bool isServerConnected() const { return _serverConnected; }
    int8_t wifiRssi() const { return _rssi; }

    // Configuration
    void setServer(const char* host, uint16_t port, const char* apiKey);
    void setDeviceId(const char* deviceId);
    void setPrinterId(int printerId) { _printerId = printerId; }
    int printerId() const { return _printerId; }

    // API actions
    bool registerDevice();
    bool sendHeartbeat();
    bool fetchPrinterStatus(PrinterStatus& out, HmsError* hmsOut, uint8_t& hmsCount);
    bool clearPlate();
    bool fetchQueue(char jobs[][64], uint8_t& count);
    bool ackHms();

    // OTA check (from heartbeat response)
    bool otaPending() const { return _otaUrl[0] != '\0'; }
    const char* otaUrl() const { return _otaUrl; }
    void clearOta() { _otaUrl[0] = '\0'; }

private:
    // WiFi state
    bool _wifiConnected = false;
    int8_t _rssi = -100;
    uint32_t _lastWifiCheck = 0;

    // Server state
    bool _serverConnected = false;
    char _serverHost[64] = {};
    uint16_t _serverPort = 8000;
    char _apiKey[128] = {};
    char _deviceId[32] = {};
    int _printerId = -1;

    // Timing
    uint32_t _lastHeartbeat = 0;
    uint32_t _lastStatusPoll = 0;

    // OTA
    char _otaUrl[256] = {};

    // Helpers
    String _buildUrl(const char* path);
    bool _httpGet(const char* url, String& response);
    bool _httpPost(const char* url, const char* body, String& response);
};
