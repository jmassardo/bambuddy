/*
 * SpoolBuddy ESP32 Firmware
 *
 * Reads NFC tags (PN5180) and scale weight (HX711), reports events to
 * the Bambuddy backend via HTTP. Full feature parity with the Raspberry Pi
 * Python daemon.
 *
 * Hardware:
 *   - ESP32-S3-DevKitC-1
 *   - PN5180 NFC reader (SPI)
 *   - HX711 load cell amplifier (2-wire GPIO)
 */

#include <Arduino.h>
#include <ArduinoJson.h>

#include "config.h"
#include "wifi_manager.h"
#include "api_client.h"
#include "nfc_reader.h"
#include "scale_reader.h"
#include "system_stats.h"
#include "ota.h"
#include "pins.h"

#define FIRMWARE_VERSION "0.1.0"

// Global instances
static Config config;
static WifiManager wifi;
static ApiClient api;
static NFCReader nfc;
static ScaleReader scale;
static OTAManager ota;
static SystemStats stats;

// Task handles
static TaskHandle_t nfcTaskHandle = nullptr;
static TaskHandle_t scaleTaskHandle = nullptr;
static TaskHandle_t heartbeatTaskHandle = nullptr;

// Shared state (protected by mutex)
static SemaphoreHandle_t stateMutex;

struct SharedState {
    bool nfcScanPaused = false;
    bool pendingWrite = false;
    int pendingWriteSpoolId = 0;
    uint8_t pendingWriteData[256] = {0};
    size_t pendingWriteLen = 0;
};
static SharedState shared;

// --- NFC Task (Core 0) ---

void nfcTask(void* param) {
    log_i("NFC task started on core %d", xPortGetCoreID());

    while (true) {
        if (shared.nfcScanPaused || !nfc.ok()) {
            vTaskDelay(pdMS_TO_TICKS(config.nfc_poll_interval_ms));
            continue;
        }

        TagEvent event = nfc.poll();

        if (event.type == TagEvent::TAG_DETECTED) {
            api.tagScanned(
                config.device_id,
                String(event.uid),
                String(event.trayUuid),
                event.sak,
                String(event.tagType)
            );
        } else if (event.type == TagEvent::TAG_REMOVED) {
            api.tagRemoved(config.device_id, String(event.uid));
        }

        // Check for pending write command
        xSemaphoreTake(stateMutex, portMAX_DELAY);
        bool shouldWrite = shared.pendingWrite && nfc.state() == NFCState::TAG_PRESENT;
        xSemaphoreGive(stateMutex);

        if (shouldWrite) {
            uint8_t sak = nfc.currentSak();
            if (sak == 0x00 || sak == 0x04) {
                xSemaphoreTake(stateMutex, portMAX_DELAY);
                int spoolId = shared.pendingWriteSpoolId;
                size_t dataLen = shared.pendingWriteLen;
                uint8_t data[256];
                memcpy(data, shared.pendingWriteData, dataLen);
                shared.pendingWrite = false;
                xSemaphoreGive(stateMutex);

                String message;
                bool success = nfc.writeNtag(data, dataLen, message);
                api.writeTagResult(config.device_id, spoolId,
                                   String(nfc.currentUid()), success, message);
            } else {
                // Incompatible tag
                xSemaphoreTake(stateMutex, portMAX_DELAY);
                int spoolId = shared.pendingWriteSpoolId;
                shared.pendingWrite = false;
                xSemaphoreGive(stateMutex);

                char msg[64];
                snprintf(msg, sizeof(msg),
                         "Incompatible tag type (SAK=0x%02X). Place an NTAG tag to write.", sak);
                api.writeTagResult(config.device_id, spoolId,
                                   String(nfc.currentUid()), false, String(msg));
            }
        }

        vTaskDelay(pdMS_TO_TICKS(config.nfc_poll_interval_ms));
    }
}

// --- Scale Task (Core 0) ---

void scaleTask(void* param) {
    log_i("Scale task started on core %d", xPortGetCoreID());

    if (!scale.ok()) {
        log_w("Scale not available, task exiting");
        vTaskDelete(nullptr);
        return;
    }

    uint32_t lastReport = 0;
    float lastReportedGrams = -9999.0f;
    const float REPORT_THRESHOLD = 2.0f;

    while (true) {
        ScaleReader::Reading reading = scale.read();

        if (reading.valid) {
            uint32_t now = millis();

            if (now - lastReport >= config.scale_report_interval_ms) {
                bool changed = (lastReportedGrams < -9000.0f) ||
                               fabsf(reading.grams - lastReportedGrams) >= REPORT_THRESHOLD;

                if (changed) {
                    api.scaleReading(config.device_id, reading.grams,
                                     reading.stable, reading.rawAdc);
                    lastReportedGrams = reading.grams;
                }
                lastReport = now;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(config.scale_read_interval_ms));
    }
}

// --- Heartbeat Task (Core 1, with WiFi) ---

void heartbeatTask(void* param) {
    log_i("Heartbeat task started on core %d", xPortGetCoreID());

    uint32_t startTime = millis();
    bool otaConfirmed = false;

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(config.heartbeat_interval_ms));

        wifi.loop();
        if (!wifi.isConnected()) continue;

        uint32_t uptimeS = (millis() - startTime) / 1000;
        stats.collect();

        JsonDocument statsDoc;
        JsonObject statsObj = statsDoc.to<JsonObject>();
        stats.toJson(statsObj);

        ApiResponse resp = api.heartbeat(
            config.device_id,
            nfc.ok(),
            scale.ok(),
            uptimeS,
            wifi.localIP(),
            FIRMWARE_VERSION,
            statsObj
        );

        if (!resp.success) continue;

        // Confirm OTA after first successful heartbeat
        if (!otaConfirmed) {
            ota.confirmFirmware();
            otaConfirmed = true;
        }

        // Process commands
        String cmd = resp.pendingCommand();

        if (cmd == "tare") {
            if (scale.ok()) {
                int32_t newOffset = scale.tare();
                api.updateTare(config.device_id, newOffset);
                config.tare_offset = newOffset;
                config.saveCalibration();
            }
            continue;
        }

        if (cmd == "write_tag") {
            int spoolId = resp.writeSpool();
            String ndefHex = resp.writeNdefHex();
            if (spoolId > 0 && ndefHex.length() > 0) {
                xSemaphoreTake(stateMutex, portMAX_DELAY);
                shared.pendingWriteSpoolId = spoolId;
                shared.pendingWriteLen = ndefHex.length() / 2;
                for (size_t i = 0; i < shared.pendingWriteLen && i < sizeof(shared.pendingWriteData); i++) {
                    char hex[3] = {ndefHex[i * 2], ndefHex[i * 2 + 1], '\0'};
                    shared.pendingWriteData[i] = (uint8_t)strtol(hex, nullptr, 16);
                }
                shared.pendingWrite = true;
                xSemaphoreGive(stateMutex);
                log_i("Write tag command received for spool %d", spoolId);
            }
            continue;
        }

        if (cmd == "reboot") {
            api.systemCommandResult(config.device_id, "reboot", true, "Rebooting");
            delay(500);
            ESP.restart();
        }

        if (cmd == "apply_system_config") {
            JsonObjectConst payload = resp.doc["pending_system_payload"];
            String newUrl = payload["backend_url"] | "";
            String newKey = payload["api_key"] | "";
            if (!newUrl.isEmpty()) {
                config.saveBackend(newUrl, newKey);
                api.systemCommandResult(config.device_id, "apply_system_config",
                                        true, "Config updated, restarting");
                delay(500);
                ESP.restart();
            } else {
                api.systemCommandResult(config.device_id, "apply_system_config",
                                        false, "Missing backend_url");
            }
            continue;
        }

        if (cmd == "run_nfc_diag" || cmd == "run_scale_diag") {
            String diagnostic = (cmd == "run_scale_diag") ? "scale" : "nfc";
            String output;
            bool success;

            if (diagnostic == "nfc") {
                uint8_t ver[2];
                if (nfc.ok()) {
                    output = "NFC reader: PN5180, SPI, OK\n";
                    output += "State: " + String(nfc.state() == NFCState::IDLE ? "IDLE" : "TAG_PRESENT");
                    success = true;
                } else {
                    output = "NFC reader: not responding";
                    success = false;
                }
            } else {
                if (scale.ok()) {
                    output = "Scale: HX711, OK\n";
                    output += "Last raw: " + String(scale.lastRaw()) + "\n";
                    output += "Tare offset: " + String(config.tare_offset) + "\n";
                    output += "Cal factor: " + String(config.calibration_factor, 6);
                    success = true;
                } else {
                    output = "Scale: HX711 not responding";
                    success = false;
                }
            }
            api.diagnosticResult(config.device_id, diagnostic, success, output);
            continue;
        }

        // OTA update check
        String fwUrl = resp.firmwareUrl();
        String fwVer = resp.firmwareVersion();
        if (!fwUrl.isEmpty() && !fwVer.isEmpty()) {
            ota.checkAndUpdate(fwUrl, fwVer);
        }

        // Sync calibration from backend
        int32_t newTare = resp.tareOffset(config.tare_offset);
        float newCal = resp.calibrationFactor(config.calibration_factor);
        if (newTare != config.tare_offset || newCal != config.calibration_factor) {
            config.tare_offset = newTare;
            config.calibration_factor = newCal;
            scale.updateCalibration(newTare, newCal);
            config.saveCalibration();
            log_i("Calibration synced: tare=%d, factor=%.6f", newTare, newCal);
        }
    }
}

// --- Setup & Loop ---

void setup() {
    Serial.begin(115200);
    delay(1000);

    log_i("SpoolBuddy ESP32 v%s starting...", FIRMWARE_VERSION);

    // Load config from NVS
    config.load();

    if (!config.isConfigured()) {
        log_w("Device not configured — starting AP for provisioning");
        wifi.beginAP("SpoolBuddy-Setup");
        // TODO: Start captive portal web server for WiFi/backend config
        // For now, configure via serial or NVS tool
        while (true) {
            delay(1000);
            if (Serial.available()) {
                // Minimal serial config interface for development
                String line = Serial.readStringUntil('\n');
                line.trim();
                if (line.startsWith("wifi:")) {
                    int comma = line.indexOf(',', 5);
                    if (comma > 5) {
                        config.saveWifi(line.substring(5, comma), line.substring(comma + 1));
                        log_i("WiFi saved");
                    }
                } else if (line.startsWith("backend:")) {
                    int comma = line.indexOf(',', 8);
                    if (comma > 8) {
                        config.saveBackend(line.substring(8, comma), line.substring(comma + 1));
                        log_i("Backend saved");
                    }
                } else if (line == "boot") {
                    config.load();
                    if (config.isConfigured()) {
                        ESP.restart();
                    } else {
                        log_w("Still not configured");
                    }
                }
            }
        }
    }

    // Connect WiFi
    wifi.begin(config.wifi_ssid, config.wifi_password, config.hostname);

    // Initialize hardware
    nfc.begin();

    scale.setTareOffset(config.tare_offset);
    scale.setCalibrationFactor(config.calibration_factor);
    scale.begin();

    // Initialize OTA
    ota.begin(FIRMWARE_VERSION);

    // Initialize API client
    api.begin(config.backend_url, config.api_key);

    // Register with backend (blocking, retries)
    log_i("Registering with backend...");
    ApiResponse reg;
    while (true) {
        reg = api.registerDevice(
            config.device_id,
            config.hostname,
            wifi.localIP(),
            FIRMWARE_VERSION,
            true,   // has_nfc
            true,   // has_scale
            config.tare_offset,
            config.calibration_factor,
            nfc.readerType(),
            nfc.connection(),
            config.backend_url
        );
        if (reg.success) break;
        log_w("Registration failed, retrying in 5s...");
        delay(5000);
        wifi.loop();
    }

    // Apply server-side calibration
    int32_t serverTare = reg.tareOffset(config.tare_offset);
    float serverCal = reg.calibrationFactor(config.calibration_factor);
    if (serverTare != config.tare_offset || serverCal != config.calibration_factor) {
        config.tare_offset = serverTare;
        config.calibration_factor = serverCal;
        scale.updateCalibration(serverTare, serverCal);
        config.saveCalibration();
    }

    log_i("Registered. Starting tasks...");

    // Create mutex for shared state
    stateMutex = xSemaphoreCreateMutex();

    // Launch FreeRTOS tasks
    // NFC and Scale on Core 0 (no WiFi interference)
    xTaskCreatePinnedToCore(nfcTask, "nfc", 8192, nullptr, 2, &nfcTaskHandle, 0);
    xTaskCreatePinnedToCore(scaleTask, "scale", 4096, nullptr, 1, &scaleTaskHandle, 0);

    // Heartbeat on Core 1 (shares with WiFi stack)
    xTaskCreatePinnedToCore(heartbeatTask, "heartbeat", 8192, nullptr, 1, &heartbeatTaskHandle, 1);

    log_i("All tasks running. SpoolBuddy ready.");
}

void loop() {
    // All work is done in FreeRTOS tasks
    vTaskDelay(pdMS_TO_TICKS(1000));
}
