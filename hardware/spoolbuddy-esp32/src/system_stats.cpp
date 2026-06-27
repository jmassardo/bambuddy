#include "system_stats.h"
#include <WiFi.h>
#include <esp_system.h>

#ifdef __cplusplus
extern "C" {
#endif
#include <esp_chip_info.h>
#ifdef __cplusplus
}
#endif

void SystemStats::collect() {
    chipTempC = temperatureRead();
    freeHeap = ESP.getFreeHeap();
    totalHeap = ESP.getHeapSize();
    freePsram = ESP.getFreePsram();
    totalPsram = ESP.getPsramSize();
    wifiRssi = WiFi.RSSI();
    uptimeS = millis() / 1000;
    cpuFreqMhz = ESP.getCpuFreqMHz();
    chipModel = ESP.getChipModel();
    chipRevision = ESP.getChipRevision();
    sdkVersion = ESP.getSdkVersion();
}

void SystemStats::toJson(JsonObject& obj) const {
    JsonObject os = obj["os"].to<JsonObject>();
    os["os"] = "ESP-IDF " + sdkVersion;
    os["arch"] = chipModel;
    os["chip_revision"] = chipRevision;

    obj["cpu_temp_c"] = chipTempC;
    obj["cpu_freq_mhz"] = cpuFreqMhz;
    obj["wifi_rssi"] = wifiRssi;

    JsonObject memory = obj["memory"].to<JsonObject>();
    memory["total_kb"] = totalHeap / 1024;
    memory["free_kb"] = freeHeap / 1024;
    memory["used_kb"] = (totalHeap - freeHeap) / 1024;
    memory["percent"] = totalHeap > 0 ? (float)(totalHeap - freeHeap) / totalHeap * 100.0f : 0;

    if (totalPsram > 0) {
        JsonObject psram = obj["psram"].to<JsonObject>();
        psram["total_kb"] = totalPsram / 1024;
        psram["free_kb"] = freePsram / 1024;
    }

    obj["system_uptime_s"] = uptimeS;
}
