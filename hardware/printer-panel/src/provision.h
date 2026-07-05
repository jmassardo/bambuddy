#pragma once

#include <Arduino.h>

// NVS keys for persistent configuration
#define NVS_NAMESPACE       "bambuddy"
#define NVS_KEY_WIFI_SSID   "wifi_ssid"
#define NVS_KEY_WIFI_PASS   "wifi_pass"
#define NVS_KEY_SERVER_HOST "srv_host"
#define NVS_KEY_SERVER_PORT "srv_port"
#define NVS_KEY_API_KEY     "api_key"
#define NVS_KEY_DEVICE_ID   "device_id"
#define NVS_KEY_PRINTER_ID  "printer_id"
#define NVS_KEY_PROVISIONED "provisioned"

struct DeviceConfig {
    char wifiSsid[64];
    char wifiPass[64];
    char serverHost[64];
    uint16_t serverPort;
    char apiKey[128];
    char deviceId[32];
    int printerId;
    bool provisioned;
};

class Provisioning {
public:
    void begin();

    // Load config from NVS. Returns true if device is provisioned.
    bool loadConfig(DeviceConfig& cfg);

    // Save config to NVS
    void saveConfig(const DeviceConfig& cfg);

    // Start captive portal AP for first-boot config
    // Blocks until user submits config, then saves and reboots.
    void startCaptivePortal();

    // Clear stored config (factory reset)
    void factoryReset();

private:
    void _handleRoot();
    void _handleSave();
    void _handleScan();
    void _handleSerialConfig(const String& json);
};
