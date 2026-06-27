#include "config.h"
#include <WiFi.h>

void Config::load() {
    _prefs.begin("spoolbuddy", true);  // read-only

    backend_url = _prefs.getString("backend_url", "");
    api_key = _prefs.getString("api_key", "");
    device_id = _prefs.getString("device_id", "");
    hostname = _prefs.getString("hostname", "");
    wifi_ssid = _prefs.getString("wifi_ssid", "");
    wifi_password = _prefs.getString("wifi_pass", "");
    tare_offset = _prefs.getInt("tare_offset", 0);
    calibration_factor = _prefs.getFloat("cal_factor", 1.0f);

    _prefs.end();

    if (device_id.isEmpty()) {
        device_id = _generateDeviceId();
        // Persist generated ID
        _prefs.begin("spoolbuddy", false);
        _prefs.putString("device_id", device_id);
        _prefs.end();
    }

    if (hostname.isEmpty()) {
        hostname = "spoolbuddy-" + device_id.substring(3, 9);
    }
}

void Config::save() {
    _prefs.begin("spoolbuddy", false);
    _prefs.putString("backend_url", backend_url);
    _prefs.putString("api_key", api_key);
    _prefs.putString("device_id", device_id);
    _prefs.putString("hostname", hostname);
    _prefs.putString("wifi_ssid", wifi_ssid);
    _prefs.putString("wifi_pass", wifi_password);
    _prefs.putInt("tare_offset", tare_offset);
    _prefs.putFloat("cal_factor", calibration_factor);
    _prefs.end();
}

void Config::saveCalibration() {
    _prefs.begin("spoolbuddy", false);
    _prefs.putInt("tare_offset", tare_offset);
    _prefs.putFloat("cal_factor", calibration_factor);
    _prefs.end();
}

void Config::saveWifi(const String& ssid, const String& password) {
    wifi_ssid = ssid;
    wifi_password = password;
    _prefs.begin("spoolbuddy", false);
    _prefs.putString("wifi_ssid", ssid);
    _prefs.putString("wifi_pass", password);
    _prefs.end();
}

void Config::saveBackend(const String& url, const String& key) {
    backend_url = url;
    api_key = key;
    _prefs.begin("spoolbuddy", false);
    _prefs.putString("backend_url", url);
    _prefs.putString("api_key", key);
    _prefs.end();
}

bool Config::isConfigured() const {
    return !backend_url.isEmpty() && !api_key.isEmpty() &&
           !wifi_ssid.isEmpty();
}

String Config::_generateDeviceId() {
    uint8_t mac[6];
    WiFi.macAddress(mac);
    char id[16];
    snprintf(id, sizeof(id), "sb-%02x%02x%02x%02x%02x%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(id);
}
