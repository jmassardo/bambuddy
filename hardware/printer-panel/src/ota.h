#pragma once

#include <Arduino.h>

// OTA status callback
using OtaProgressCallback = void (*)(uint8_t percent);
using OtaCompleteCallback = void (*)(bool success, const char* message);

class OtaUpdater {
public:
    // Attempt OTA update from the given URL.
    // Returns true if update was initiated (device will reboot on success).
    // Returns false if download/verify failed (device continues running).
    bool update(const char* firmwareUrl, const char* apiKey,
                OtaProgressCallback onProgress = nullptr,
                OtaCompleteCallback onComplete = nullptr);

    // Check if we just rebooted from a successful OTA
    bool justUpdated() const;

    // Mark the current firmware as valid (prevent rollback)
    void confirmUpdate();

private:
    bool _downloadAndFlash(const char* url, const char* apiKey,
                           OtaProgressCallback onProgress);
};
