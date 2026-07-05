#pragma once
// SpoolBuddy v3 — Touch Display Driver
// Hosyond 4.0" ST7796S 320x480 + FT6336U capacitive touch
// Uses LovyanGFX with band-rendering (no PSRAM)

#include <Arduino.h>
#include "tft_config.h"

// Layout constants for 320×480 portrait
#define TFT_WIDTH          320
#define TFT_HEIGHT         480
#define STATUS_BAR_H        36
#define CONTENT_Y           36
#define CONTENT_H          (TFT_HEIGHT - STATUS_BAR_H)
#define TOUCH_BTN_H         60   // Minimum touch target height
#define TOUCH_BTN_MARGIN     8

// Band sprite height for RAM-efficient rendering
// 320 × 40 × 2 bytes = 25,600 bytes per band
#define SPRITE_BAND_H       40

// Colors (RGB565)
#define C_BG           0x1082  // Dark charcoal
#define C_STATUS_BG    0x2104  // Slightly lighter
#define C_TEXT         0xFFFF  // White
#define C_TEXT_DIM     0x8410  // Gray
#define C_ACCENT       0x34DF  // Cyan/teal
#define C_SUCCESS      0x07E0  // Green
#define C_WARNING      0xFD20  // Orange
#define C_ERROR        0xF800  // Red

// Display auto-blank timeout
#define DISPLAY_BLANK_MS  60000

// Touch gesture thresholds
#define TOUCH_LONG_PRESS_MS  600
#define TOUCH_SWIPE_MIN_PX    40

enum class Screen {
    BOOT,
    IDLE,
    TAG_MATCHED,
    TAG_UNKNOWN,
    TAG_WRITING,
    TAG_WRITE_OK,
    TAG_WRITE_FAIL,
    OTA_PROGRESS,
    ERROR,
    PROVISIONING,
    MENU,
};

enum class TouchEvent {
    NONE,
    TAP,
    LONG_PRESS,
    SWIPE_UP,
    SWIPE_DOWN,
    SWIPE_LEFT,
    SWIPE_RIGHT,
};

struct TouchPoint {
    int16_t x;
    int16_t y;
};

struct SpoolInfo {
    char material[16];
    char colorName[20];
    char brand[20];
    uint32_t colorHex;     // Actual filament color (for swatch)
    float weightUsed;
    float labelWeight;
};

class TouchDisplay {
public:
    void begin();
    void loop();

    // Status bar
    void setWifiRSSI(int8_t rssi);
    void setWifiConnected(bool connected);
    void setBackendConnected(bool connected);
    void setNfcOk(bool ok);
    void setScaleOk(bool ok);

    // Screens
    void showBoot(const char* version);
    void showIdle(float weightGrams, bool stable);
    void showTagMatched(const SpoolInfo& spool);
    void showTagUnknown(const char* uid);
    void showTagWriting(int spoolId);
    void showTagWriteResult(bool success, const char* msg);
    void showOtaProgress(int percent, const char* version);
    void showError(const char* message);
    void showProvisioning(const char* apName);

    // Display control
    void setBrightness(uint8_t percent);
    void setBlankTimeout(uint32_t ms);
    void wake();

    // Touch input (call from main loop)
    TouchEvent pollTouch();
    TouchPoint lastTouchPoint() const { return _lastTouch; }

private:
    LGFX _tft;
    LGFX_Sprite _band;  // Band sprite for partial rendering

    Screen _screen = Screen::BOOT;
    Screen _prevScreen = Screen::BOOT;
    uint32_t _msgExpiry = 0;
    uint32_t _lastActivity = 0;
    uint32_t _blankTimeout = DISPLAY_BLANK_MS;
    bool _blanked = false;
    bool _needsRedraw = true;

    // Status state
    int8_t _wifiRSSI = -100;
    bool _wifiConnected = false;
    bool _backendConnected = false;
    bool _nfcOk = false;
    bool _scaleOk = false;

    // Content state
    float _weightGrams = 0.0f;
    bool _weightStable = false;
    SpoolInfo _spoolInfo = {};
    char _msgBuf[64] = {};
    char _uidBuf[24] = {};
    int _otaPercent = 0;
    char _otaVersion[16] = {};

    // Touch state
    TouchPoint _lastTouch = {0, 0};
    TouchPoint _touchStart = {0, 0};
    uint32_t _touchStartMs = 0;
    bool _touching = false;

    // Rendering
    void _render();
    void _drawStatusBar();
    void _drawBoot();
    void _drawIdle();
    void _drawTagMatched();
    void _drawTagUnknown();
    void _drawTagWriting();
    void _drawTagWriteResult();
    void _drawOta();
    void _drawError();
    void _drawProvisioning();

    // Helpers
    void _drawColorSwatch(int x, int y, int w, int h, uint32_t color);
    void _drawProgressBar(int x, int y, int w, int h, int percent, uint16_t color);
    void _drawButton(int x, int y, int w, int h, const char* label, uint16_t bg);
    void _drawWifiIcon(int x, int y, int8_t rssi);

    // Touch processing
    TouchEvent _processTouch();
};
