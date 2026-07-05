#include "display.h"
#include "pins.h"

const char* const Display::MENU_LABELS[MENU_ITEMS] = {
    "Plate Clear",
    "HMS Errors",
    "Spool Assign",
    "Queue",
    "Settings",
    "Back",
};

void Display::begin() {
    _u8g2.begin();
    _u8g2.setContrast(180);
    _lastActivity = millis();
    showScreen(Screen::BOOT);
}

void Display::loop() {
    uint32_t now = millis();

    // Screen blank timeout
    if (DISPLAY_TIMEOUT_MS > 0 && !_blanked && (now - _lastActivity) > DISPLAY_TIMEOUT_MS) {
        _u8g2.clearBuffer();
        _u8g2.sendBuffer();
        _blanked = true;
        return;
    }
    if (_blanked) return;

    // Expire toast
    if (_toastExpiry > 0 && now > _toastExpiry) {
        _toastExpiry = 0;
    }

    _render();
}

void Display::wake() {
    _lastActivity = millis();
    if (_blanked) {
        _blanked = false;
        _render();
    }
}

void Display::showScreen(Screen s) {
    _screen = s;
    _lastActivity = millis();
    if (s == Screen::MENU) _menuIdx = 0;
    if (s == Screen::HMS_ERROR) _hmsViewIdx = 0;
    _render();
}

void Display::updatePrinterStatus(const PrinterStatus& status) {
    memcpy(&_printer, &status, sizeof(PrinterStatus));
}

void Display::setHmsErrors(const HmsError* errors, uint8_t count) {
    _hmsCount = min(count, MAX_HMS);
    memcpy(_hmsErrors, errors, _hmsCount * sizeof(HmsError));
}

void Display::setQueueJobs(const char jobs[][64], uint8_t count) {
    _queueCount = min(count, MAX_QUEUE);
    for (uint8_t i = 0; i < _queueCount; i++) {
        strncpy(_queueJobs[i], jobs[i], 63);
        _queueJobs[i][63] = '\0';
    }
}

void Display::setWifiStatus(bool connected, int8_t rssi) {
    _wifiConnected = connected;
    _wifiRssi = rssi;
}

void Display::setServerStatus(bool connected) {
    _serverConnected = connected;
}

void Display::menuScroll(int delta) {
    wake();
    if (_screen == Screen::MENU) {
        _menuIdx = (_menuIdx + MENU_ITEMS + delta) % MENU_ITEMS;
        _render();
    } else if (_screen == Screen::HMS_ERROR && _hmsCount > 0) {
        _hmsViewIdx = (_hmsViewIdx + _hmsCount + delta) % _hmsCount;
        _render();
    }
}

void Display::menuSelect() {
    wake();
    // Handled externally via menuIndex()
}

void Display::showToast(const char* msg, uint16_t durationMs) {
    strncpy(_toastMsg, msg, sizeof(_toastMsg) - 1);
    _toastMsg[sizeof(_toastMsg) - 1] = '\0';
    _toastExpiry = millis() + durationMs;
    _render();
}

// --- Rendering ---

void Display::_render() {
    _u8g2.clearBuffer();
    _drawStatusBar();

    switch (_screen) {
        case Screen::BOOT:          _drawBoot(); break;
        case Screen::HOME:          _drawHome(); break;
        case Screen::MENU:          _drawMenu(); break;
        case Screen::PLATE_CLEAR:   _drawPlateClear(); break;
        case Screen::HMS_ERROR:     _drawHmsError(); break;
        case Screen::SPOOL_ASSIGN:  _drawSpoolAssign(); break;
        case Screen::QUEUE_PEEK:    _drawQueuePeek(); break;
        case Screen::SETTINGS:      _drawSettings(); break;
        case Screen::PROVISIONING:  _drawProvisioning(); break;
        case Screen::ERROR:         _drawError(); break;
    }

    if (_toastExpiry > 0) _drawToast();

    _u8g2.sendBuffer();
}

void Display::_drawStatusBar() {
    // WiFi icon (left)
    _u8g2.setFont(u8g2_font_open_iconic_embedded_1x_t);
    if (_wifiConnected) {
        _u8g2.drawGlyph(0, 9, 0x0050);  // WiFi icon
    } else {
        _u8g2.drawGlyph(0, 9, 0x0047);  // No-signal icon
    }

    // Server dot (next to WiFi)
    if (_serverConnected) {
        _u8g2.drawDisc(14, 5, 2);  // filled = connected
    } else {
        _u8g2.drawCircle(14, 5, 2);  // outline = disconnected
    }

    // Printer name (center)
    _u8g2.setFont(u8g2_font_6x10_tr);
    uint8_t nameW = _u8g2.getStrWidth(_printer.name);
    _u8g2.drawStr((DISPLAY_WIDTH - nameW) / 2, 9, _printer.name);

    // Separator line
    _u8g2.drawHLine(0, STATUS_BAR_HEIGHT - 1, DISPLAY_WIDTH);
}

void Display::_drawBoot() {
    _u8g2.setFont(u8g2_font_helvB10_tr);
    _u8g2.drawStr(20, 36, "BamBuddy");
    _u8g2.setFont(u8g2_font_6x10_tr);
    _u8g2.drawStr(28, 52, "Panel v" FIRMWARE_VERSION);
}

void Display::_drawHome() {
    uint8_t y = CONTENT_Y_START + 4;

    if (strcmp(_printer.state, "RUNNING") == 0) {
        // Printing: show progress bar + time remaining
        _u8g2.setFont(u8g2_font_6x10_tr);

        // Job name (truncated)
        char truncJob[22];
        strncpy(truncJob, _printer.jobName, 21);
        truncJob[21] = '\0';
        _u8g2.drawStr(0, y + 10, truncJob);

        // Progress bar
        uint8_t barY = y + 16;
        uint8_t barW = (uint8_t)(_printer.progress * 1.20f);  // 120px max
        _u8g2.drawFrame(2, barY, 124, 12);
        _u8g2.drawBox(4, barY + 2, barW, 8);

        // Percentage + time
        char pctStr[8];
        snprintf(pctStr, sizeof(pctStr), "%d%%", (int)_printer.progress);
        _u8g2.drawStr(2, barY + 26, pctStr);

        char timeStr[16];
        if (_printer.timeRemaining > 60) {
            snprintf(timeStr, sizeof(timeStr), "%dh%dm", _printer.timeRemaining / 60, _printer.timeRemaining % 60);
        } else {
            snprintf(timeStr, sizeof(timeStr), "%dm", _printer.timeRemaining);
        }
        uint8_t tw = _u8g2.getStrWidth(timeStr);
        _u8g2.drawStr(DISPLAY_WIDTH - tw - 2, barY + 26, timeStr);

    } else if (strcmp(_printer.state, "FINISH") == 0 || _printer.awaitingClear) {
        // Print done — prompt for plate clear
        _u8g2.setFont(u8g2_font_helvB10_tr);
        _u8g2.drawStr(16, y + 18, "Print Done!");
        _u8g2.setFont(u8g2_font_6x10_tr);
        _u8g2.drawStr(8, y + 36, "Press CLR to clear");

    } else if (strcmp(_printer.state, "IDLE") == 0) {
        // Idle
        _u8g2.setFont(u8g2_font_helvB10_tr);
        _u8g2.drawStr(44, y + 22, "Idle");

    } else if (strcmp(_printer.state, "PAUSE") == 0) {
        _u8g2.setFont(u8g2_font_helvB10_tr);
        _u8g2.drawStr(32, y + 18, "Paused");
        _u8g2.setFont(u8g2_font_6x10_tr);
        char pctStr[8];
        snprintf(pctStr, sizeof(pctStr), "%d%%", (int)_printer.progress);
        _u8g2.drawStr(54, y + 36, pctStr);

    } else if (strcmp(_printer.state, "FAILED") == 0) {
        _u8g2.setFont(u8g2_font_helvB10_tr);
        _u8g2.drawStr(32, y + 22, "FAILED");

    } else {
        // Unknown / connecting
        _u8g2.setFont(u8g2_font_6x10_tr);
        _u8g2.drawStr(16, y + 22, "Connecting...");
    }
}

void Display::_drawMenu() {
    uint8_t y = CONTENT_Y_START + 2;
    _u8g2.setFont(u8g2_font_6x10_tr);

    // Show 4 visible items with scroll indicator
    uint8_t startIdx = (_menuIdx > 2) ? _menuIdx - 2 : 0;
    if (startIdx + 4 > MENU_ITEMS) startIdx = MENU_ITEMS - 4;

    for (uint8_t i = 0; i < 4 && (startIdx + i) < MENU_ITEMS; i++) {
        uint8_t idx = startIdx + i;
        uint8_t itemY = y + (i * 13) + 10;

        if (idx == _menuIdx) {
            _u8g2.drawBox(0, itemY - 9, DISPLAY_WIDTH, 12);
            _u8g2.setDrawColor(0);
            _u8g2.drawStr(4, itemY, MENU_LABELS[idx]);
            _u8g2.setDrawColor(1);
        } else {
            _u8g2.drawStr(4, itemY, MENU_LABELS[idx]);
        }
    }
}

void Display::_drawPlateClear() {
    uint8_t y = CONTENT_Y_START;
    _u8g2.setFont(u8g2_font_helvB10_tr);
    _u8g2.drawStr(12, y + 18, "Clear Plate?");
    _u8g2.setFont(u8g2_font_6x10_tr);
    _u8g2.drawStr(8, y + 36, "ENC=confirm BACK=no");
}

void Display::_drawHmsError() {
    uint8_t y = CONTENT_Y_START;
    if (_hmsCount == 0) {
        _u8g2.setFont(u8g2_font_6x10_tr);
        _u8g2.drawStr(20, y + 24, "No HMS errors");
        return;
    }

    const HmsError& e = _hmsErrors[_hmsViewIdx];
    _u8g2.setFont(u8g2_font_6x10_tr);

    // Error counter
    char counter[12];
    snprintf(counter, sizeof(counter), "%d/%d", _hmsViewIdx + 1, _hmsCount);
    _u8g2.drawStr(0, y + 10, counter);

    // Severity indicator
    const char* sev = "???";
    switch (e.severity) {
        case 1: sev = "FATAL"; break;
        case 2: sev = "SERIOUS"; break;
        case 3: sev = "COMMON"; break;
        case 4: sev = "INFO"; break;
    }
    uint8_t sw = _u8g2.getStrWidth(sev);
    _u8g2.drawStr(DISPLAY_WIDTH - sw, y + 10, sev);

    // Code
    _u8g2.drawStr(0, y + 24, e.code);

    // Description (wrap if needed)
    _u8g2.drawStr(0, y + 38, e.shortDesc);

    // Nav hint
    _u8g2.setFont(u8g2_font_5x7_tr);
    _u8g2.drawStr(0, y + 50, "Scroll:nav ENC:ack");
}

void Display::_drawSpoolAssign() {
    uint8_t y = CONTENT_Y_START;
    _u8g2.setFont(u8g2_font_6x10_tr);
    _u8g2.drawStr(4, y + 14, "Spool Assign");
    _u8g2.drawStr(4, y + 30, "(Coming soon)");
}

void Display::_drawQueuePeek() {
    uint8_t y = CONTENT_Y_START;
    _u8g2.setFont(u8g2_font_6x10_tr);

    if (_queueCount == 0) {
        _u8g2.drawStr(20, y + 24, "Queue empty");
        return;
    }

    _u8g2.drawStr(0, y + 10, "Next jobs:");
    for (uint8_t i = 0; i < _queueCount && i < 4; i++) {
        char line[24];
        snprintf(line, sizeof(line), "%d. %.18s", i + 1, _queueJobs[i]);
        _u8g2.drawStr(2, y + 22 + (i * 11), line);
    }
}

void Display::_drawSettings() {
    uint8_t y = CONTENT_Y_START;
    _u8g2.setFont(u8g2_font_6x10_tr);
    _u8g2.drawStr(4, y + 14, "Settings");
    _u8g2.drawStr(4, y + 30, "FW: " FIRMWARE_VERSION);
    _u8g2.drawStr(4, y + 42, "(Coming soon)");
}

void Display::_drawProvisioning() {
    uint8_t y = CONTENT_Y_START;
    _u8g2.setFont(u8g2_font_helvB10_tr);
    _u8g2.drawStr(20, y + 14, "Setup Mode");
    _u8g2.setFont(u8g2_font_6x10_tr);
    _u8g2.drawStr(4, y + 30, "Connect to WiFi:");
    _u8g2.drawStr(4, y + 42, "BamBuddy-Panel");
}

void Display::_drawError() {
    uint8_t y = CONTENT_Y_START;
    _u8g2.setFont(u8g2_font_helvB10_tr);
    _u8g2.drawStr(32, y + 22, "ERROR");
}

void Display::_drawToast() {
    // Overlay a toast message at the bottom
    uint8_t boxY = DISPLAY_HEIGHT - 16;
    _u8g2.setDrawColor(0);
    _u8g2.drawBox(0, boxY, DISPLAY_WIDTH, 16);
    _u8g2.setDrawColor(1);
    _u8g2.drawFrame(0, boxY, DISPLAY_WIDTH, 16);
    _u8g2.setFont(u8g2_font_6x10_tr);
    uint8_t tw = _u8g2.getStrWidth(_toastMsg);
    _u8g2.drawStr((DISPLAY_WIDTH - tw) / 2, boxY + 12, _toastMsg);
}
