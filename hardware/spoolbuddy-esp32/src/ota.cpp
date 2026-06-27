#include "ota.h"
#include <HTTPClient.h>
#include <Update.h>
#include <esp_ota_ops.h>

void OTAManager::begin(const String& currentVersion) {
    _currentVersion = currentVersion;

    // Mark current partition as valid if we booted successfully
    // (prevents rollback on next reboot)
    const esp_partition_t* running = esp_ota_get_running_partition();
    esp_ota_img_states_t state;
    if (esp_ota_get_state_partition(running, &state) == ESP_OK) {
        if (state == ESP_OTA_IMG_PENDING_VERIFY) {
            log_i("OTA: New firmware booted, waiting for confirmation...");
        }
    }
}

void OTAManager::confirmFirmware() {
    esp_ota_mark_app_valid_cancel_rollback();
}

bool OTAManager::checkAndUpdate(const String& firmwareUrl, const String& newVersion) {
    if (firmwareUrl.isEmpty() || newVersion.isEmpty()) return false;
    if (newVersion == _currentVersion) return false;

    log_i("OTA: Updating from %s to %s", _currentVersion.c_str(), newVersion.c_str());
    log_i("OTA: Downloading from %s", firmwareUrl.c_str());

    _updating = true;

    HTTPClient http;
    http.begin(firmwareUrl);
    http.setTimeout(30000);

    int code = http.GET();
    if (code != 200) {
        log_e("OTA: Download failed with HTTP %d", code);
        _updating = false;
        return false;
    }

    int contentLength = http.getSize();
    if (contentLength <= 0) {
        log_e("OTA: Invalid content length: %d", contentLength);
        http.end();
        _updating = false;
        return false;
    }

    if (!Update.begin(contentLength)) {
        log_e("OTA: Not enough space for update (%d bytes)", contentLength);
        http.end();
        _updating = false;
        return false;
    }

    WiFiClient* stream = http.getStreamPtr();
    size_t written = Update.writeStream(*stream);

    if (written != (size_t)contentLength) {
        log_e("OTA: Written %d of %d bytes", written, contentLength);
        Update.abort();
        http.end();
        _updating = false;
        return false;
    }

    if (!Update.end(true)) {
        log_e("OTA: Update finalization failed");
        http.end();
        _updating = false;
        return false;
    }

    http.end();
    log_i("OTA: Update successful, rebooting...");
    delay(500);
    ESP.restart();

    return true;  // Never reached
}
