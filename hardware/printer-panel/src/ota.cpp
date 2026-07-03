#include "ota.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>
#include <esp_ota_ops.h>

bool OtaUpdater::update(const char* firmwareUrl, const char* apiKey,
                         OtaProgressCallback onProgress,
                         OtaCompleteCallback onComplete) {
    Serial.printf("[OTA] Starting update from: %s\n", firmwareUrl);

    bool success = _downloadAndFlash(firmwareUrl, apiKey, onProgress);

    if (onComplete) {
        onComplete(success, success ? "Update successful, rebooting..." : "Update failed");
    }

    if (success) {
        Serial.println("[OTA] Update successful! Rebooting in 1s...");
        delay(1000);
        ESP.restart();
    }

    return success;
}

bool OtaUpdater::justUpdated() const {
    // Check if we're running from the non-factory partition (i.e., an OTA partition)
    const esp_partition_t* running = esp_ota_get_running_partition();
    const esp_partition_t* boot = esp_ota_get_boot_partition();
    // If running != configured boot, we might have just updated
    return running != nullptr && boot != nullptr && running == boot;
}

void OtaUpdater::confirmUpdate() {
    // Mark current partition as valid (prevents automatic rollback)
    esp_ota_mark_app_valid_cancel_rollback();
    Serial.println("[OTA] Firmware confirmed as valid.");
}

bool OtaUpdater::_downloadAndFlash(const char* url, const char* apiKey,
                                    OtaProgressCallback onProgress) {
    HTTPClient http;
    http.begin(url);
    http.setTimeout(30000);  // 30s timeout for firmware download
    if (apiKey && apiKey[0]) {
        http.addHeader("X-API-Key", apiKey);
    }

    int httpCode = http.GET();
    if (httpCode != 200) {
        Serial.printf("[OTA] HTTP GET failed: %d\n", httpCode);
        http.end();
        return false;
    }

    int contentLength = http.getSize();
    if (contentLength <= 0) {
        Serial.println("[OTA] Invalid content length");
        http.end();
        return false;
    }

    Serial.printf("[OTA] Firmware size: %d bytes\n", contentLength);

    if (!Update.begin(contentLength)) {
        Serial.printf("[OTA] Not enough space: %s\n", Update.errorString());
        http.end();
        return false;
    }

    WiFiClient* stream = http.getStreamPtr();
    uint8_t buf[1024];
    int written = 0;
    int lastPercent = -1;

    while (http.connected() && written < contentLength) {
        int available = stream->available();
        if (available > 0) {
            int toRead = min(available, (int)sizeof(buf));
            int bytesRead = stream->readBytes(buf, toRead);
            if (bytesRead > 0) {
                Update.write(buf, bytesRead);
                written += bytesRead;

                // Progress callback
                int percent = (written * 100) / contentLength;
                if (percent != lastPercent && onProgress) {
                    onProgress((uint8_t)percent);
                    lastPercent = percent;
                }
            }
        }
        delay(1);
    }

    http.end();

    if (written != contentLength) {
        Serial.printf("[OTA] Size mismatch: wrote %d / expected %d\n", written, contentLength);
        Update.abort();
        return false;
    }

    if (!Update.end(true)) {
        Serial.printf("[OTA] Update finalize failed: %s\n", Update.errorString());
        return false;
    }

    Serial.println("[OTA] Update written successfully!");
    return true;
}
