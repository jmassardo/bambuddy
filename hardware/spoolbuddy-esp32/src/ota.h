#pragma once

#include <Arduino.h>

class OTAManager {
public:
    void begin(const String& currentVersion);

    // Check if update is available and perform it
    // Returns true if update started (device will reboot)
    bool checkAndUpdate(const String& firmwareUrl, const String& newVersion);

    // Mark current firmware as valid (call after successful heartbeat)
    void confirmFirmware();

    const String& currentVersion() const { return _currentVersion; }
    bool updateInProgress() const { return _updating; }

private:
    String _currentVersion;
    bool _updating = false;
};
