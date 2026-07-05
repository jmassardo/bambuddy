#include "display.h"
#include <Wire.h>

void Display::begin() {
    // Auto-detect I2C address (some OLEDs use 0x3D instead of 0x3C)
    Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
    delay(100);
    uint8_t addr = 0x3C;  // default
    for (uint8_t a = 0x3C; a <= 0x3D; a++) {
        Wire.beginTransmission(a);
        if (Wire.endTransmission() == 0) {
            addr = a;
            break;
        }
    }
    Wire.end();

    _u8g2.setI2CAddress(addr * 2);  // U8g2 uses 8-bit address
    _u8g2.begin();
    _u8g2.setContrast(200);
    _lastActivity = millis();
}

void Display::loop() {
    uint32_t now = millis();

    // Auto-blank after timeout
    if (_blankTimeout > 0 && !_blanked && (now - _lastActivity) > _blankTimeout) {
        _u8g2.clearBuffer();
        _u8g2.sendBuffer();
        _blanked = true;
        return;
    }
    if (_blanked) return;

    // Expire transient messages → return to idle
    if (_msgExpiry > 0 && now > _msgExpiry) {
        _msgExpiry = 0;
        _screen = DisplayScreen::IDLE;
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

// --- Status bar setters ---

void Display::setWifiRSSI(int8_t rssi) { _wifiRSSI = rssi; wake(); }
void Display::setWifiConnected(bool connected) { _wifiConnected = connected; wake(); }
void Display::setBackendConnected(bool connected) { _backendConnected = connected; wake(); }
void Display::setNfcOk(bool ok) { _nfcOk = ok; wake(); }
void Display::setScaleOk(bool ok) { _scaleOk = ok; wake(); }

// --- Content screens ---

void Display::showBoot(const char* version) {
    _screen = DisplayScreen::BOOT;
    strncpy(_msgBuf, version, sizeof(_msgBuf) - 1);
    _msgExpiry = 0;
    wake();
}

void Display::showIdle(float weightGrams, bool stable) {
    if (_screen != DisplayScreen::IDLE && _msgExpiry > 0) return;  // don't override transient
    _screen = DisplayScreen::IDLE;
    _weightGrams = weightGrams;
    _weightStable = stable;
    wake();
}

void Display::showTagMatched(const SpoolInfo& spool) {
    _screen = DisplayScreen::TAG_MATCHED;
    _spoolInfo = spool;
    _msgExpiry = millis() + DISPLAY_MSG_DURATION_MS;
    wake();
}

void Display::showTagUnknown(const char* uid) {
    _screen = DisplayScreen::TAG_UNKNOWN;
    strncpy(_uidBuf, uid, sizeof(_uidBuf) - 1);
    _msgExpiry = millis() + DISPLAY_MSG_DURATION_MS;
    wake();
}

void Display::showTagWriting(int spoolId) {
    _screen = DisplayScreen::TAG_WRITING;
    snprintf(_msgBuf, sizeof(_msgBuf), "Spool #%d", spoolId);
    _msgExpiry = 0;
    wake();
}

void Display::showTagWriteResult(bool success, const char* message) {
    _screen = success ? DisplayScreen::TAG_WRITE_OK : DisplayScreen::TAG_WRITE_FAIL;
    strncpy(_msgBuf, message, sizeof(_msgBuf) - 1);
    _msgExpiry = millis() + DISPLAY_MSG_DURATION_MS;
    wake();
}

void Display::showOtaProgress(int percent, const char* version) {
    _screen = DisplayScreen::OTA_PROGRESS;
    _otaPercent = percent;
    strncpy(_otaVersion, version, sizeof(_otaVersion) - 1);
    _msgExpiry = 0;
    wake();
}

void Display::showError(const char* message) {
    _screen = DisplayScreen::ERROR;
    strncpy(_msgBuf, message, sizeof(_msgBuf) - 1);
    _msgExpiry = millis() + DISPLAY_MSG_DURATION_MS;
    wake();
}

void Display::showProvisioning(const char* apName) {
    _screen = DisplayScreen::PROVISIONING;
    strncpy(_msgBuf, apName, sizeof(_msgBuf) - 1);
    _msgExpiry = 0;
    wake();
}

void Display::setBrightness(uint8_t percent) {
    uint8_t val = (uint8_t)((uint16_t)percent * 255 / 100);
    _u8g2.setContrast(val);
}

void Display::setBlankTimeout(uint32_t ms) {
    _blankTimeout = ms;
}

// --- Rendering ---

void Display::_render() {
    _u8g2.clearBuffer();
    _drawStatusBar();

    switch (_screen) {
        case DisplayScreen::BOOT:           _drawContentBoot(); break;
        case DisplayScreen::IDLE:           _drawContentIdle(); break;
        case DisplayScreen::TAG_MATCHED:    _drawContentTagMatched(); break;
        case DisplayScreen::TAG_UNKNOWN:    _drawContentTagUnknown(); break;
        case DisplayScreen::TAG_WRITING:    _drawContentTagWriting(); break;
        case DisplayScreen::TAG_WRITE_OK:
        case DisplayScreen::TAG_WRITE_FAIL: _drawContentTagWriteResult(); break;
        case DisplayScreen::OTA_PROGRESS:   _drawContentOta(); break;
        case DisplayScreen::ERROR:          _drawContentError(); break;
        case DisplayScreen::PROVISIONING:   _drawContentProvisioning(); break;
    }

    _u8g2.sendBuffer();
}

// --- Status Bar (yellow zone, rows 0–15) ---

void Display::_drawStatusBar() {
    // WiFi icon + RSSI
    _drawWifiIcon(0, 0);

    // Backend connection indicator
    _u8g2.setFont(u8g2_font_open_iconic_embedded_1x_t);
    if (_backendConnected) {
        _u8g2.drawGlyph(22, 12, 0x0050);  // link/cloud icon
    } else {
        _u8g2.drawGlyph(22, 12, 0x0047);  // disconnected
    }

    // NFC status
    _u8g2.setFont(u8g2_font_5x7_tr);
    if (_nfcOk) {
        _u8g2.drawStr(38, 11, "NFC");
    } else {
        _u8g2.drawStr(38, 11, "---");
    }

    // Scale status
    if (_scaleOk) {
        _u8g2.drawStr(58, 11, "SCL");
    } else {
        _u8g2.drawStr(58, 11, "---");
    }

    // Separator line between yellow and blue zones
    _u8g2.drawHLine(0, 15, 128);
}

void Display::_drawWifiIcon(int x, int y) {
    // Draw signal strength bars (4 levels)
    int bars = 0;
    if (_wifiConnected) {
        if (_wifiRSSI > -50) bars = 4;
        else if (_wifiRSSI > -60) bars = 3;
        else if (_wifiRSSI > -70) bars = 2;
        else bars = 1;
    }

    for (int i = 0; i < 4; i++) {
        int bx = x + i * 4;
        int bh = 3 + i * 3;  // heights: 3, 6, 9, 12
        int by = y + 12 - bh;
        if (i < bars) {
            _u8g2.drawBox(bx, by, 3, bh);
        } else {
            _u8g2.drawFrame(bx, by, 3, bh);
        }
    }
}

// --- Content Area (blue zone, rows 16–63) ---

void Display::_drawContentBoot() {
    _u8g2.setFont(u8g2_font_helvB14_tr);
    const char* title = "SpoolBuddy";
    int w = _u8g2.getStrWidth(title);
    _u8g2.drawStr((128 - w) / 2, 38, title);

    _u8g2.setFont(u8g2_font_5x7_tr);
    char ver[24];
    snprintf(ver, sizeof(ver), "v%s", _msgBuf);
    int vw = _u8g2.getStrWidth(ver);
    _u8g2.drawStr((128 - vw) / 2, 55, ver);
}

void Display::_drawContentIdle() {
    // Large weight display
    char weightStr[16];
    if (_weightGrams < 0.5f && _weightGrams > -0.5f) {
        snprintf(weightStr, sizeof(weightStr), "0 g");
    } else if (_weightGrams >= 1000.0f) {
        snprintf(weightStr, sizeof(weightStr), "%.1f kg", _weightGrams / 1000.0f);
    } else {
        snprintf(weightStr, sizeof(weightStr), "%.0f g", _weightGrams);
    }

    _u8g2.setFont(u8g2_font_helvB18_tr);
    int w = _u8g2.getStrWidth(weightStr);
    _u8g2.drawStr((128 - w) / 2, 44, weightStr);

    // Stability indicator
    _u8g2.setFont(u8g2_font_5x7_tr);
    if (_weightStable) {
        _u8g2.drawStr(50, 58, "STABLE");
    } else {
        _u8g2.drawStr(44, 58, "measuring...");
    }
}

void Display::_drawContentTagMatched() {
    // Material + brand
    _u8g2.setFont(u8g2_font_helvB10_tr);
    char topLine[40];
    snprintf(topLine, sizeof(topLine), "%s %s", _spoolInfo.brand, _spoolInfo.material);
    int w1 = _u8g2.getStrWidth(topLine);
    _u8g2.drawStr((128 - w1) / 2, 32, topLine);

    // Color name
    _u8g2.setFont(u8g2_font_helvR10_tr);
    int w2 = _u8g2.getStrWidth(_spoolInfo.colorName);
    _u8g2.drawStr((128 - w2) / 2, 47, _spoolInfo.colorName);

    // Remaining weight bar
    float remaining = _spoolInfo.labelWeight - _spoolInfo.weightUsed;
    if (remaining < 0) remaining = 0;
    int percent = (_spoolInfo.labelWeight > 0)
        ? (int)(remaining / _spoolInfo.labelWeight * 100.0f)
        : 0;
    _drawProgressBar(10, 52, 90, 8, percent);

    // Percentage text
    char pctStr[8];
    snprintf(pctStr, sizeof(pctStr), "%d%%", percent);
    _u8g2.setFont(u8g2_font_5x7_tr);
    _u8g2.drawStr(104, 60, pctStr);
}

void Display::_drawContentTagUnknown() {
    _u8g2.setFont(u8g2_font_helvB10_tr);
    const char* title = "Unknown Tag";
    int w = _u8g2.getStrWidth(title);
    _u8g2.drawStr((128 - w) / 2, 34, title);

    _u8g2.setFont(u8g2_font_5x7_tr);
    int uw = _u8g2.getStrWidth(_uidBuf);
    _u8g2.drawStr((128 - uw) / 2, 48, _uidBuf);

    const char* hint = "Register in Bambuddy";
    int hw = _u8g2.getStrWidth(hint);
    _u8g2.drawStr((128 - hw) / 2, 60, hint);
}

void Display::_drawContentTagWriting() {
    _u8g2.setFont(u8g2_font_helvB10_tr);
    const char* title = "Writing Tag...";
    int w = _u8g2.getStrWidth(title);
    _u8g2.drawStr((128 - w) / 2, 34, title);

    _u8g2.setFont(u8g2_font_helvR10_tr);
    int mw = _u8g2.getStrWidth(_msgBuf);
    _u8g2.drawStr((128 - mw) / 2, 52, _msgBuf);
}

void Display::_drawContentTagWriteResult() {
    bool success = (_screen == DisplayScreen::TAG_WRITE_OK);

    _u8g2.setFont(u8g2_font_helvB10_tr);
    const char* title = success ? "Write OK!" : "Write Failed";
    int w = _u8g2.getStrWidth(title);
    _u8g2.drawStr((128 - w) / 2, 34, title);

    // Show checkmark or X
    _u8g2.setFont(u8g2_font_open_iconic_check_2x_t);
    _u8g2.drawGlyph(56, 60, success ? 0x0040 : 0x0042);
}

void Display::_drawContentOta() {
    _u8g2.setFont(u8g2_font_helvB10_tr);
    const char* title = "Updating...";
    int w = _u8g2.getStrWidth(title);
    _u8g2.drawStr((128 - w) / 2, 32, title);

    _drawProgressBar(10, 38, 108, 10, _otaPercent);

    _u8g2.setFont(u8g2_font_5x7_tr);
    char info[32];
    snprintf(info, sizeof(info), "v%s  %d%%", _otaVersion, _otaPercent);
    int iw = _u8g2.getStrWidth(info);
    _u8g2.drawStr((128 - iw) / 2, 60, info);
}

void Display::_drawContentError() {
    _u8g2.setFont(u8g2_font_open_iconic_embedded_2x_t);
    _u8g2.drawGlyph(4, 40, 0x0047);  // warning icon

    _u8g2.setFont(u8g2_font_helvR10_tr);
    // Word-wrap not worth it on 128px — just truncate
    _u8g2.drawStr(24, 34, _msgBuf);
}

void Display::_drawContentProvisioning() {
    _u8g2.setFont(u8g2_font_helvB10_tr);
    const char* title = "Setup Mode";
    int w = _u8g2.getStrWidth(title);
    _u8g2.drawStr((128 - w) / 2, 32, title);

    _u8g2.setFont(u8g2_font_5x7_tr);
    char line[40];
    snprintf(line, sizeof(line), "WiFi: %s", _msgBuf);
    int lw = _u8g2.getStrWidth(line);
    _u8g2.drawStr((128 - lw) / 2, 47, line);

    const char* hint = "Connect to configure";
    int hw = _u8g2.getStrWidth(hint);
    _u8g2.drawStr((128 - hw) / 2, 60, hint);
}

// --- Helpers ---

void Display::_drawProgressBar(int x, int y, int w, int h, int percent) {
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;

    _u8g2.drawFrame(x, y, w, h);
    int fillW = (w - 2) * percent / 100;
    if (fillW > 0) {
        _u8g2.drawBox(x + 1, y + 1, fillW, h - 2);
    }
}
