// SpoolBuddy v3 — Touch Display Implementation
// Band-rendering approach for ESP32-S3 without PSRAM

#include "touch_display.h"

void TouchDisplay::begin() {
    _tft.init();
    _tft.setRotation(0);  // Portrait: 320 wide × 480 tall
    _tft.setBrightness(200);
    _tft.fillScreen(C_BG);

    // Allocate band sprite (320 × SPRITE_BAND_H)
    // Memory: 320 × 40 × 2 = 25,600 bytes
    _band.createSprite(TFT_WIDTH, SPRITE_BAND_H);

    _lastActivity = millis();
    _needsRedraw = true;
}

void TouchDisplay::loop() {
    uint32_t now = millis();

    // Auto-blank after timeout
    if (_blankTimeout > 0 && !_blanked && (now - _lastActivity) > _blankTimeout) {
        _tft.setBrightness(0);
        _blanked = true;
        return;
    }

    // Auto-expire transient messages
    if (_msgExpiry > 0 && now > _msgExpiry) {
        _msgExpiry = 0;
        _screen = Screen::IDLE;
        _needsRedraw = true;
    }

    // Only redraw when needed (saves SPI bandwidth and CPU)
    if (_needsRedraw) {
        _render();
        _needsRedraw = false;
    }
}

// ─── Touch Input ────────────────────────────────────────────────────

TouchEvent TouchDisplay::pollTouch() {
    lgfx::touch_point_t tp;
    int touchCount = _tft.getTouch(&tp, 1);

    if (touchCount > 0) {
        _lastTouch = {(int16_t)tp.x, (int16_t)tp.y};

        if (!_touching) {
            // Touch start
            _touching = true;
            _touchStart = _lastTouch;
            _touchStartMs = millis();
        }

        // Wake from blank on any touch
        if (_blanked) {
            wake();
            return TouchEvent::NONE;  // Consume the wake touch
        }

        _lastActivity = millis();
        return TouchEvent::NONE;  // Still touching, wait for release
    }

    // Touch released
    if (_touching) {
        _touching = false;
        uint32_t duration = millis() - _touchStartMs;
        int16_t dx = _lastTouch.x - _touchStart.x;
        int16_t dy = _lastTouch.y - _touchStart.y;
        int16_t adx = abs(dx);
        int16_t ady = abs(dy);

        // Classify gesture
        if (adx > TOUCH_SWIPE_MIN_PX || ady > TOUCH_SWIPE_MIN_PX) {
            // Swipe
            if (adx > ady) {
                return dx > 0 ? TouchEvent::SWIPE_RIGHT : TouchEvent::SWIPE_LEFT;
            } else {
                return dy > 0 ? TouchEvent::SWIPE_DOWN : TouchEvent::SWIPE_UP;
            }
        } else if (duration >= TOUCH_LONG_PRESS_MS) {
            return TouchEvent::LONG_PRESS;
        } else {
            return TouchEvent::TAP;
        }
    }

    return TouchEvent::NONE;
}

// ─── Screen Setters ─────────────────────────────────────────────────

void TouchDisplay::showBoot(const char* version) {
    _screen = Screen::BOOT;
    strncpy(_otaVersion, version, sizeof(_otaVersion) - 1);
    _needsRedraw = true;
}

void TouchDisplay::showIdle(float weightGrams, bool stable) {
    _weightGrams = weightGrams;
    _weightStable = stable;
    if (_screen != Screen::IDLE) {
        _screen = Screen::IDLE;
    }
    _needsRedraw = true;
}

void TouchDisplay::showTagMatched(const SpoolInfo& spool) {
    _spoolInfo = spool;
    _screen = Screen::TAG_MATCHED;
    _msgExpiry = millis() + 8000;  // Show longer on big screen
    _needsRedraw = true;
}

void TouchDisplay::showTagUnknown(const char* uid) {
    strncpy(_uidBuf, uid, sizeof(_uidBuf) - 1);
    _screen = Screen::TAG_UNKNOWN;
    _msgExpiry = millis() + 5000;
    _needsRedraw = true;
}

void TouchDisplay::showTagWriting(int spoolId) {
    snprintf(_msgBuf, sizeof(_msgBuf), "Writing spool #%d...", spoolId);
    _screen = Screen::TAG_WRITING;
    _needsRedraw = true;
}

void TouchDisplay::showTagWriteResult(bool success, const char* msg) {
    strncpy(_msgBuf, msg, sizeof(_msgBuf) - 1);
    _screen = success ? Screen::TAG_WRITE_OK : Screen::TAG_WRITE_FAIL;
    _msgExpiry = millis() + 4000;
    _needsRedraw = true;
}

void TouchDisplay::showOtaProgress(int percent, const char* version) {
    _otaPercent = percent;
    strncpy(_otaVersion, version, sizeof(_otaVersion) - 1);
    _screen = Screen::OTA_PROGRESS;
    _needsRedraw = true;
}

void TouchDisplay::showError(const char* message) {
    strncpy(_msgBuf, message, sizeof(_msgBuf) - 1);
    _screen = Screen::ERROR;
    _msgExpiry = millis() + 6000;
    _needsRedraw = true;
}

void TouchDisplay::showProvisioning(const char* apName) {
    strncpy(_msgBuf, apName, sizeof(_msgBuf) - 1);
    _screen = Screen::PROVISIONING;
    _needsRedraw = true;
}

// ─── Display Control ────────────────────────────────────────────────

void TouchDisplay::setBrightness(uint8_t percent) {
    uint8_t val = map(percent, 0, 100, 0, 255);
    _tft.setBrightness(val);
}

void TouchDisplay::setBlankTimeout(uint32_t ms) {
    _blankTimeout = ms;
}

void TouchDisplay::wake() {
    _blanked = false;
    _tft.setBrightness(200);
    _lastActivity = millis();
    _needsRedraw = true;
}

// ─── Status Bar Setters ─────────────────────────────────────────────

void TouchDisplay::setWifiRSSI(int8_t rssi) { _wifiRSSI = rssi; _needsRedraw = true; }
void TouchDisplay::setWifiConnected(bool c) { _wifiConnected = c; _needsRedraw = true; }
void TouchDisplay::setBackendConnected(bool c) { _backendConnected = c; _needsRedraw = true; }
void TouchDisplay::setNfcOk(bool ok) { _nfcOk = ok; _needsRedraw = true; }
void TouchDisplay::setScaleOk(bool ok) { _scaleOk = ok; _needsRedraw = true; }

// ─── Rendering ──────────────────────────────────────────────────────

void TouchDisplay::_render() {
    _drawStatusBar();

    switch (_screen) {
        case Screen::BOOT:         _drawBoot(); break;
        case Screen::IDLE:         _drawIdle(); break;
        case Screen::TAG_MATCHED:  _drawTagMatched(); break;
        case Screen::TAG_UNKNOWN:  _drawTagUnknown(); break;
        case Screen::TAG_WRITING:  _drawTagWriting(); break;
        case Screen::TAG_WRITE_OK:
        case Screen::TAG_WRITE_FAIL: _drawTagWriteResult(); break;
        case Screen::OTA_PROGRESS: _drawOta(); break;
        case Screen::ERROR:        _drawError(); break;
        case Screen::PROVISIONING: _drawProvisioning(); break;
        default: break;
    }
}

void TouchDisplay::_drawStatusBar() {
    _tft.fillRect(0, 0, TFT_WIDTH, STATUS_BAR_H, C_STATUS_BG);

    // WiFi icon (left)
    _drawWifiIcon(8, 8, _wifiRSSI);

    // Backend connection dot
    _tft.fillCircle(50, STATUS_BAR_H / 2, 5,
        _backendConnected ? C_SUCCESS : C_ERROR);

    // NFC status
    _tft.setTextColor(_nfcOk ? C_SUCCESS : C_TEXT_DIM);
    _tft.setTextSize(1);
    _tft.setCursor(70, 12);
    _tft.print("NFC");

    // Scale status
    _tft.setTextColor(_scaleOk ? C_SUCCESS : C_TEXT_DIM);
    _tft.setCursor(110, 12);
    _tft.print("SCALE");
}

void TouchDisplay::_drawBoot() {
    _tft.fillRect(0, STATUS_BAR_H, TFT_WIDTH, CONTENT_H, C_BG);
    _tft.setTextColor(C_ACCENT);
    _tft.setTextSize(3);
    _tft.setTextDatum(lgfx::middle_center);
    _tft.drawString("SpoolBuddy", TFT_WIDTH / 2, 200);

    _tft.setTextColor(C_TEXT_DIM);
    _tft.setTextSize(2);
    _tft.drawString(_otaVersion, TFT_WIDTH / 2, 260);
    _tft.setTextDatum(lgfx::top_left);
}

void TouchDisplay::_drawIdle() {
    _tft.fillRect(0, STATUS_BAR_H, TFT_WIDTH, CONTENT_H, C_BG);

    // Large weight reading
    char buf[16];
    snprintf(buf, sizeof(buf), "%.1f g", _weightGrams);

    _tft.setTextDatum(lgfx::middle_center);
    _tft.setTextColor(_weightStable ? C_TEXT : C_TEXT_DIM);
    _tft.setTextSize(4);
    _tft.drawString(buf, TFT_WIDTH / 2, 180);

    // Stability indicator
    _tft.setTextSize(2);
    _tft.setTextColor(C_TEXT_DIM);
    _tft.drawString(_weightStable ? "STABLE" : "MEASURING...", TFT_WIDTH / 2, 240);

    // Instruction
    _tft.setTextSize(1);
    _tft.setTextColor(C_ACCENT);
    _tft.drawString("Place spool on NFC reader", TFT_WIDTH / 2, 380);
    _tft.setTextDatum(lgfx::top_left);
}

void TouchDisplay::_drawTagMatched() {
    _tft.fillRect(0, STATUS_BAR_H, TFT_WIDTH, CONTENT_H, C_BG);

    // Color swatch (big visual indicator of filament color)
    _drawColorSwatch(20, CONTENT_Y + 20, 80, 80, _spoolInfo.colorHex);

    // Spool details to the right of swatch
    int textX = 120;
    _tft.setTextColor(C_TEXT);
    _tft.setTextDatum(lgfx::top_left);

    _tft.setTextSize(2);
    _tft.setCursor(textX, CONTENT_Y + 25);
    _tft.print(_spoolInfo.material);

    _tft.setTextSize(2);
    _tft.setTextColor(C_ACCENT);
    _tft.setCursor(textX, CONTENT_Y + 55);
    _tft.print(_spoolInfo.colorName);

    _tft.setTextSize(1);
    _tft.setTextColor(C_TEXT_DIM);
    _tft.setCursor(textX, CONTENT_Y + 85);
    _tft.print(_spoolInfo.brand);

    // Weight info below
    char buf[48];
    snprintf(buf, sizeof(buf), "Used: %.0fg / %.0fg",
             _spoolInfo.weightUsed, _spoolInfo.labelWeight);
    _tft.setTextSize(2);
    _tft.setTextColor(C_TEXT);
    _tft.setTextDatum(lgfx::middle_center);
    _tft.drawString(buf, TFT_WIDTH / 2, 250);

    // Usage progress bar
    int pct = (_spoolInfo.labelWeight > 0)
        ? (int)((_spoolInfo.weightUsed / _spoolInfo.labelWeight) * 100)
        : 0;
    _drawProgressBar(20, 280, TFT_WIDTH - 40, 20, pct, C_ACCENT);
    _tft.setTextDatum(lgfx::top_left);
}

void TouchDisplay::_drawTagUnknown() {
    _tft.fillRect(0, STATUS_BAR_H, TFT_WIDTH, CONTENT_H, C_BG);
    _tft.setTextDatum(lgfx::middle_center);

    _tft.setTextColor(C_WARNING);
    _tft.setTextSize(3);
    _tft.drawString("Unknown Tag", TFT_WIDTH / 2, 180);

    _tft.setTextColor(C_TEXT_DIM);
    _tft.setTextSize(1);
    _tft.drawString(_uidBuf, TFT_WIDTH / 2, 240);

    _tft.setTextColor(C_TEXT);
    _tft.setTextSize(2);
    _tft.drawString("Tap to register", TFT_WIDTH / 2, 320);
    _tft.setTextDatum(lgfx::top_left);
}

void TouchDisplay::_drawTagWriting() {
    _tft.fillRect(0, STATUS_BAR_H, TFT_WIDTH, CONTENT_H, C_BG);
    _tft.setTextDatum(lgfx::middle_center);
    _tft.setTextColor(C_ACCENT);
    _tft.setTextSize(2);
    _tft.drawString(_msgBuf, TFT_WIDTH / 2, 220);

    // Animated dots would go here in a real impl
    _tft.setTextDatum(lgfx::top_left);
}

void TouchDisplay::_drawTagWriteResult() {
    _tft.fillRect(0, STATUS_BAR_H, TFT_WIDTH, CONTENT_H, C_BG);
    _tft.setTextDatum(lgfx::middle_center);

    bool ok = (_screen == Screen::TAG_WRITE_OK);
    _tft.setTextColor(ok ? C_SUCCESS : C_ERROR);
    _tft.setTextSize(3);
    _tft.drawString(ok ? "SUCCESS" : "FAILED", TFT_WIDTH / 2, 180);

    _tft.setTextColor(C_TEXT_DIM);
    _tft.setTextSize(2);
    _tft.drawString(_msgBuf, TFT_WIDTH / 2, 250);
    _tft.setTextDatum(lgfx::top_left);
}

void TouchDisplay::_drawOta() {
    _tft.fillRect(0, STATUS_BAR_H, TFT_WIDTH, CONTENT_H, C_BG);
    _tft.setTextDatum(lgfx::middle_center);

    _tft.setTextColor(C_TEXT);
    _tft.setTextSize(2);
    _tft.drawString("Updating Firmware", TFT_WIDTH / 2, 160);

    _tft.setTextColor(C_ACCENT);
    _tft.setTextSize(2);
    _tft.drawString(_otaVersion, TFT_WIDTH / 2, 200);

    _drawProgressBar(30, 260, TFT_WIDTH - 60, 30, _otaPercent, C_SUCCESS);

    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", _otaPercent);
    _tft.setTextColor(C_TEXT);
    _tft.setTextSize(2);
    _tft.drawString(buf, TFT_WIDTH / 2, 310);
    _tft.setTextDatum(lgfx::top_left);
}

void TouchDisplay::_drawError() {
    _tft.fillRect(0, STATUS_BAR_H, TFT_WIDTH, CONTENT_H, C_BG);
    _tft.setTextDatum(lgfx::middle_center);

    _tft.setTextColor(C_ERROR);
    _tft.setTextSize(3);
    _tft.drawString("ERROR", TFT_WIDTH / 2, 160);

    _tft.setTextColor(C_TEXT);
    _tft.setTextSize(2);
    _tft.drawString(_msgBuf, TFT_WIDTH / 2, 240);
    _tft.setTextDatum(lgfx::top_left);
}

void TouchDisplay::_drawProvisioning() {
    _tft.fillRect(0, STATUS_BAR_H, TFT_WIDTH, CONTENT_H, C_BG);
    _tft.setTextDatum(lgfx::middle_center);

    _tft.setTextColor(C_ACCENT);
    _tft.setTextSize(2);
    _tft.drawString("WiFi Setup", TFT_WIDTH / 2, 140);

    _tft.setTextColor(C_TEXT);
    _tft.setTextSize(2);
    _tft.drawString("Connect to:", TFT_WIDTH / 2, 200);

    _tft.setTextColor(C_WARNING);
    _tft.setTextSize(3);
    _tft.drawString(_msgBuf, TFT_WIDTH / 2, 260);

    _tft.setTextColor(C_TEXT_DIM);
    _tft.setTextSize(1);
    _tft.drawString("Then open 192.168.4.1", TFT_WIDTH / 2, 340);
    _tft.setTextDatum(lgfx::top_left);
}

// ─── Helper Drawing Functions ───────────────────────────────────────

void TouchDisplay::_drawColorSwatch(int x, int y, int w, int h, uint32_t color) {
    // Convert 24-bit RGB to RGB565
    uint16_t c565 = _tft.color888(
        (color >> 16) & 0xFF,
        (color >> 8) & 0xFF,
        color & 0xFF
    );
    _tft.fillRoundRect(x, y, w, h, 8, c565);
    _tft.drawRoundRect(x, y, w, h, 8, C_TEXT_DIM);
}

void TouchDisplay::_drawProgressBar(int x, int y, int w, int h, int percent, uint16_t color) {
    percent = constrain(percent, 0, 100);
    int fill = (w * percent) / 100;

    _tft.drawRoundRect(x, y, w, h, 4, C_TEXT_DIM);
    if (fill > 0) {
        _tft.fillRoundRect(x + 2, y + 2, fill - 4, h - 4, 3, color);
    }
}

void TouchDisplay::_drawButton(int x, int y, int w, int h, const char* label, uint16_t bg) {
    _tft.fillRoundRect(x, y, w, h, 8, bg);
    _tft.setTextColor(C_TEXT);
    _tft.setTextDatum(lgfx::middle_center);
    _tft.setTextSize(2);
    _tft.drawString(label, x + w / 2, y + h / 2);
    _tft.setTextDatum(lgfx::top_left);
}

void TouchDisplay::_drawWifiIcon(int x, int y, int8_t rssi) {
    uint16_t color;
    if (!_wifiConnected) {
        color = C_ERROR;
    } else if (rssi > -50) {
        color = C_SUCCESS;
    } else if (rssi > -70) {
        color = C_WARNING;
    } else {
        color = C_ERROR;
    }

    // Simple WiFi arc representation
    _tft.fillCircle(x + 10, y + 18, 3, color);
    if (rssi > -80) _tft.drawArc(x + 10, y + 18, 8, 6, 225, 315, color);
    if (rssi > -65) _tft.drawArc(x + 10, y + 18, 13, 11, 225, 315, color);
    if (rssi > -50) _tft.drawArc(x + 10, y + 18, 18, 16, 225, 315, color);
}
