// BamBuddy Printer Panel — ESP32-S3 Super Mini
// Per-printer control panel: OLED + encoder + 2 buttons
// Communicates with BamBuddy server for printer status, plate clear, HMS errors

#include <Arduino.h>
#include <WiFi.h>
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
        Serial.println("Device not provisioned — starting captive portal");
        display.showScreen(Screen::PROVISIONING);
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
