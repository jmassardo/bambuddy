#pragma once

#include <Arduino.h>
#include <Preferences.h>

struct Config {
    String backend_url;
    String api_key;
    String device_id;
    String hostname;
    String wifi_ssid;
    String wifi_password;

    // Scale calibration (persisted in NVS)
    int32_t tare_offset = 0;
    float calibration_factor = 1.0f;

    // Timing
    uint32_t nfc_poll_interval_ms = 300;
    uint32_t scale_read_interval_ms = 100;
    uint32_t scale_report_interval_ms = 1000;
    uint32_t heartbeat_interval_ms = 10000;

    // Stability
    float stability_threshold = 2.0f;
    float stability_window_s = 1.0f;

    void load();
    void save();
    void saveCalibration();
    void saveWifi(const String& ssid, const String& password);
    void saveBackend(const String& url, const String& key);
    bool isConfigured() const;

private:
    Preferences _prefs;
    String _generateDeviceId();
};
