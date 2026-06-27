#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

struct SystemStats {
    float chipTempC;
    uint32_t freeHeap;
    uint32_t totalHeap;
    uint32_t freePsram;
    uint32_t totalPsram;
    int32_t wifiRssi;
    uint32_t uptimeS;
    uint8_t cpuFreqMhz;
    String chipModel;
    uint8_t chipRevision;
    String sdkVersion;

    void collect();
    void toJson(JsonObject& obj) const;
};
