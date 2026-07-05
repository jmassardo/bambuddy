#pragma once

#include <Arduino.h>
#include <U8g2lib.h>
#include "pins.h"

// SSD1306 128x64 two-color display layout:
//   Yellow zone: rows 0–15  (16px) — status bar
//   Blue zone:   rows 16–63 (48px) — main content

#define DISPLAY_WIDTH      128
#define DISPLAY_HEIGHT      64
#define STATUS_BAR_HEIGHT   16
#define CONTENT_Y_START     16
#define CONTENT_HEIGHT      48

// How long to show transient messages (tag matched, error, etc.)
#define DISPLAY_MSG_DURATION_MS  4000

// Screen blank timeout (0 = never blank)
#define DISPLAY_BLANK_DEFAULT_MS 60000

enum class DisplayScreen {
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
};

struct SpoolInfo {
    char material[16];
    char colorName[20];
    char brand[20];
    float weightUsed;
    float labelWeight;
};

class Display {
public:
    void begin();
    void loop();

    // Status bar updates
    void setWifiRSSI(int8_t rssi);
    void setWifiConnected(bool connected);
    void setBackendConnected(bool connected);
    void setNfcOk(bool ok);
    void setScaleOk(bool ok);

    // Content screens
    void showBoot(const char* version);
    void showIdle(float weightGrams, bool stable);
    void showTagMatched(const SpoolInfo& spool);
    void showTagUnknown(const char* uid);
    void showTagWriting(int spoolId);
    void showTagWriteResult(bool success, const char* message);
    void showOtaProgress(int percent, const char* version);
    void showError(const char* message);
    void showProvisioning(const char* apName);

    // Display control
    void setBrightness(uint8_t percent);
    void setBlankTimeout(uint32_t ms);
    void wake();

private:
    // SW I2C with explicit pins — more reliable across boards
    U8G2_SSD1306_128X64_NONAME_F_SW_I2C _u8g2{U8G2_R0, PIN_OLED_SCL, PIN_OLED_SDA, U8X8_PIN_NONE};

    DisplayScreen _screen = DisplayScreen::BOOT;
    uint32_t _msgExpiry = 0;
    uint32_t _lastActivity = 0;
    uint32_t _blankTimeout = DISPLAY_BLANK_DEFAULT_MS;
    bool _blanked = false;

    // Status bar state
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

    void _render();
    void _drawStatusBar();
    void _drawContentBoot();
    void _drawContentIdle();
    void _drawContentTagMatched();
    void _drawContentTagUnknown();
    void _drawContentTagWriting();
    void _drawContentTagWriteResult();
    void _drawContentOta();
    void _drawContentError();
    void _drawContentProvisioning();
    void _drawWifiIcon(int x, int y);
    void _drawProgressBar(int x, int y, int w, int h, int percent);
};
