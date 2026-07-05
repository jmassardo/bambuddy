// BamBuddy Printer Panel — ESP32-S3 Super Mini
// Per-printer control panel: OLED + encoder + 2 buttons
// Communicates with BamBuddy server for printer status, plate clear, HMS errors

#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include "config.h"
#include "pins.h"
#include "display.h"
#include "input.h"
#include "api_client.h"
#include "provision.h"
#include "ota.h"

Display display;
Input input;
ApiClient network;
Provisioning provisioning;
OtaUpdater otaUpdater;
DeviceConfig deviceCfg = {};

// State
uint32_t lastStatusPoll = 0;
bool registered = false;
PrinterStatus printerStatus = {};
HmsError hmsErrors[8] = {};
uint8_t hmsCount = 0;

void handleInput(InputEvent evt);
void pollPrinterStatus();
void handleMenuAction(uint8_t menuIdx);

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=== BamBuddy Printer Panel v" FIRMWARE_VERSION " ===");

    // LED indicator
    pinMode(PIN_LED, OUTPUT);
    digitalWrite(PIN_LED, HIGH);  // OFF (active low)

    // Init display first (shows status during boot)
    display.begin();
    input.begin();
    provisioning.begin();

    // Confirm OTA if we just updated
    if (otaUpdater.justUpdated()) {
        otaUpdater.confirmUpdate();
        Serial.println("Firmware update confirmed!");
    }

    // Load config from NVS
    if (!provisioning.loadConfig(deviceCfg)) {
        Serial.println("Device not provisioned.");
        Serial.println("Option 1: Send JSON config via serial within 10s");
        Serial.println("  Format: {\"ssid\":\"X\",\"pass\":\"X\",\"host\":\"X\",\"port\":8000,\"key\":\"X\",\"id\":\"panel-001\",\"printer\":3}");
        Serial.println("Option 2: Connect to 'BamBuddy-Panel' WiFi and open 192.168.4.1");

        // Wait 10s for serial JSON config
        display.showScreen(Screen::PROVISIONING);
        uint32_t waitStart = millis();
        String serialBuf = "";
        while (millis() - waitStart < 10000) {
            if (Serial.available()) {
                char c = Serial.read();
                if (c == '\n' || c == '\r') {
                    if (serialBuf.length() > 5) break;  // Got a line
                } else {
                    serialBuf += c;
                }
            }
            // Blink LED during wait
            digitalWrite(PIN_LED, (millis() / 250) % 2 == 0 ? LOW : HIGH);
            delay(1);
        }
        digitalWrite(PIN_LED, HIGH);  // LED off

        if (serialBuf.length() > 5 && serialBuf.indexOf('{') >= 0) {
            // Parse JSON config from serial
            Serial.println("Received serial config, parsing...");
            JsonDocument doc;
            if (deserializeJson(doc, serialBuf) == DeserializationError::Ok) {
                const char* ssid = doc["ssid"] | "";
                const char* pass = doc["pass"] | "";
                const char* host = doc["host"] | "";
                uint16_t port = doc["port"] | 8000;
                const char* key = doc["key"] | "";
                const char* devid = doc["id"] | "panel-001";
                int printer = doc["printer"] | -1;

                strncpy(deviceCfg.wifiSsid, ssid, sizeof(deviceCfg.wifiSsid) - 1);
                strncpy(deviceCfg.wifiPass, pass, sizeof(deviceCfg.wifiPass) - 1);
                strncpy(deviceCfg.serverHost, host, sizeof(deviceCfg.serverHost) - 1);
                deviceCfg.serverPort = port;
                strncpy(deviceCfg.apiKey, key, sizeof(deviceCfg.apiKey) - 1);
                strncpy(deviceCfg.deviceId, devid, sizeof(deviceCfg.deviceId) - 1);
                deviceCfg.printerId = printer;
                deviceCfg.provisioned = true;

                provisioning.saveConfig(deviceCfg);
                Serial.println("Config saved! Rebooting...");
                delay(1000);
                ESP.restart();
                return;
            } else {
                Serial.println("JSON parse error! Starting captive portal...");
            }
        }

        // No serial config received — fall through to captive portal
        Serial.println("No serial config. Starting captive portal...");
        provisioning.startCaptivePortal();  // Blocks until config saved + reboot
        return;  // Never reached (reboot happens)
    }

    // Check for factory reset: hold BACK + CLR during boot
    if (digitalRead(PIN_BTN_BACK) == LOW && digitalRead(PIN_BTN_CLR) == LOW) {
        Serial.println("Factory reset requested (BACK+CLR held)!");
        display.showToast("Factory Reset!", 3000);
        display.loop();
        delay(2000);
        provisioning.factoryReset();  // Clears NVS and reboots
        return;
    }

    // Connect to WiFi
    Serial.printf("Connecting to WiFi: %s\n", deviceCfg.wifiSsid);
    network.setServer(deviceCfg.serverHost, deviceCfg.serverPort, deviceCfg.apiKey);
    network.setDeviceId(deviceCfg.deviceId);
    network.setPrinterId(deviceCfg.printerId);
    network.begin(deviceCfg.wifiSsid, deviceCfg.wifiPass);

    if (network.isWifiConnected()) {
        Serial.printf("WiFi connected! IP: %s RSSI: %d\n",
                      WiFi.localIP().toString().c_str(), network.wifiRssi());
        display.setWifiStatus(true, network.wifiRssi());

        // Register with server
        if (network.registerDevice()) {
            Serial.println("Device registered with server");
            registered = true;
            display.setServerStatus(true);
        } else {
            Serial.println("WARNING: Registration failed");
        }
    } else {
        Serial.println("WiFi connection failed!");
        display.setWifiStatus(false, -100);
    }

    // Transition to home screen
    display.showScreen(Screen::HOME);
    Serial.println("Setup complete — entering main loop");
}

void loop() {
    // Serial command handler (factory reset, diagnostics)
    if (Serial.available()) {
        static String cmdBuf;
        while (Serial.available()) {
            char c = Serial.read();
            if (c == '\n' || c == '\r') {
                cmdBuf.trim();
                if (cmdBuf == "reset") {
                    Serial.println("Factory reset...");
                    provisioning.factoryReset();
                } else if (cmdBuf == "info") {
                    Serial.printf("SSID='%s' host='%s:%d' id='%s' printer=%d\n",
                        deviceCfg.wifiSsid, deviceCfg.serverHost,
                        deviceCfg.serverPort, deviceCfg.deviceId, deviceCfg.printerId);
                } else if (cmdBuf.length() > 0) {
                    Serial.printf("Unknown cmd: '%s' (try: reset, info)\n", cmdBuf.c_str());
                }
                cmdBuf = "";
            } else {
                cmdBuf += c;
            }
        }
    }

    // Network housekeeping (heartbeat, WiFi reconnect)
    network.loop();

    // Update display connectivity indicators
    display.setWifiStatus(network.isWifiConnected(), network.wifiRssi());
    display.setServerStatus(network.isServerConnected());

    // Check for OTA update (delivered via heartbeat response)
    if (network.otaPending()) {
        Serial.printf("[OTA] Update available: %s\n", network.otaUrl());
        display.showToast("Updating FW...", 30000);
        display.loop();
        otaUpdater.update(network.otaUrl(), deviceCfg.apiKey,
            [](uint8_t percent) {
                Serial.printf("[OTA] %d%%\n", percent);
            },
            [](bool success, const char* msg) {
                Serial.printf("[OTA] %s: %s\n", success ? "OK" : "FAIL", msg);
            });
        // If we get here, OTA failed (success = reboot)
        network.clearOta();
        display.showToast("OTA failed!", 3000);
    }

    // Poll printer status periodically
    uint32_t now = millis();
    if (network.isServerConnected() && (now - lastStatusPoll) > STATUS_POLL_INTERVAL) {
        lastStatusPoll = now;
        pollPrinterStatus();
    }

    // Process input events
    InputEvent evt = input.poll();
    if (evt != InputEvent::NONE) {
        display.wake();
        handleInput(evt);
    }

    // Display refresh
    display.loop();

    // Small yield for WiFi stack
    delay(5);
}

void pollPrinterStatus() {
    PrinterStatus newStatus = {};
    HmsError newHms[8] = {};
    uint8_t newHmsCount = 0;

    if (network.fetchPrinterStatus(newStatus, newHms, newHmsCount)) {
        // Check for new HMS errors → auto-popup
        if (newHmsCount > 0 && newHmsCount != hmsCount &&
            display.currentScreen() == Screen::HOME) {
            display.showScreen(Screen::HMS_ERROR);
        }

        memcpy(&printerStatus, &newStatus, sizeof(PrinterStatus));
        memcpy(hmsErrors, newHms, sizeof(hmsErrors));
        hmsCount = newHmsCount;

        display.updatePrinterStatus(printerStatus);
        display.setHmsErrors(hmsErrors, hmsCount);
    }
}

void handleInput(InputEvent evt) {
    Screen current = display.currentScreen();

    switch (evt) {
        case InputEvent::ENC_CW:
            display.menuScroll(1);
            break;

        case InputEvent::ENC_CCW:
            display.menuScroll(-1);
            break;

        case InputEvent::ENC_PRESS:
            if (current == Screen::HOME) {
                // Enter menu
                display.showScreen(Screen::MENU);
            } else if (current == Screen::MENU) {
                // Select menu item
                handleMenuAction(display.menuIndex());
            } else if (current == Screen::PLATE_CLEAR) {
                // Confirm plate clear
                if (network.clearPlate()) {
                    display.showToast("Plate cleared!");
                } else {
                    display.showToast("Clear failed!");
                }
                display.showScreen(Screen::HOME);
            } else if (current == Screen::HMS_ERROR) {
                // Acknowledge HMS error
                if (network.ackHms()) {
                    display.showToast("HMS acknowledged");
                }
                display.showScreen(Screen::HOME);
            }
            break;

        case InputEvent::BTN_BACK:
            if (current != Screen::HOME) {
                display.showScreen(Screen::HOME);
            }
            break;

        case InputEvent::BTN_CLR:
            // Quick plate clear — go to confirm screen
            if (printerStatus.awaitingClear || strcmp(printerStatus.state, "FINISH") == 0) {
                display.showScreen(Screen::PLATE_CLEAR);
            } else {
                display.showToast("No plate to clear");
            }
            break;

        case InputEvent::BTN_CLR_LONG:
            // Long press CLR = immediate clear (skip confirm)
            if (printerStatus.awaitingClear || strcmp(printerStatus.state, "FINISH") == 0) {
                if (network.clearPlate()) {
                    display.showToast("Plate cleared!");
                } else {
                    display.showToast("Clear failed!");
                }
            }
            break;

        default:
            break;
    }
}

void handleMenuAction(uint8_t menuIdx) {
    switch (menuIdx) {
        case 0: // Plate Clear
            display.showScreen(Screen::PLATE_CLEAR);
            break;
        case 1: // HMS Errors
            display.showScreen(Screen::HMS_ERROR);
            break;
        case 2: // Spool Assign
            display.showScreen(Screen::SPOOL_ASSIGN);
            break;
        case 3: // Queue
            {
                char jobs[5][64] = {};
                uint8_t count = 0;
                if (network.fetchQueue(jobs, count)) {
                    display.setQueueJobs(jobs, count);
                }
                display.showScreen(Screen::QUEUE_PEEK);
            }
            break;
        case 4: // Settings
            display.showScreen(Screen::SETTINGS);
            break;
        case 5: // Back
            display.showScreen(Screen::HOME);
            break;
    }
}
