#pragma once

#include <Arduino.h>
#include <U8g2lib.h>
#include "config.h"

// Screen IDs matching the UI simulator flow
enum class Screen {
    BOOT,
    HOME,           // Printer status (idle/printing/done)
    MENU,           // Main menu list
    PLATE_CLEAR,    // Confirm plate clear
    HMS_ERROR,      // HMS error detail
    SPOOL_ASSIGN,   // AMS slot → spool picker
    QUEUE_PEEK,     // Next queued jobs
    SETTINGS,       // Device settings
    PROVISIONING,   // WiFi setup mode
    ERROR,          // Fatal error
};

struct PrinterStatus {
    char name[32];
    char state[16];       // IDLE, RUNNING, PAUSE, FINISH, FAILED
    float progress;       // 0-100
    int timeRemaining;    // minutes
    char jobName[64];
    bool awaitingClear;
    int hmsCount;
};

struct HmsError {
    char code[24];
    char shortDesc[48];
    int severity;         // 1=fatal, 2=serious, 3=common, 4=info
};

class Display {
public:
    void begin();
    void loop();
    void wake();

    // Screen transitions
    void showScreen(Screen s);
    Screen currentScreen() const { return _screen; }

    // Data updates
    void updatePrinterStatus(const PrinterStatus& status);
    void setHmsErrors(const HmsError* errors, uint8_t count);
    void setQueueJobs(const char jobs[][64], uint8_t count);
    void setWifiStatus(bool connected, int8_t rssi);
    void setServerStatus(bool connected);

    // Menu interaction
    void menuScroll(int delta);        // encoder rotation
    void menuSelect();                 // encoder press
    uint8_t menuIndex() const { return _menuIdx; }

    // Feedback
    void showToast(const char* msg, uint16_t durationMs = 2000);

private:
    U8G2_SSD1306_128X64_NONAME_F_HW_I2C _u8g2{U8G2_R0, U8X8_PIN_NONE};

    Screen _screen = Screen::BOOT;
    uint32_t _lastActivity = 0;
    bool _blanked = false;

    // Status bar state
    bool _wifiConnected = false;
    int8_t _wifiRssi = -100;
    bool _serverConnected = false;

    // Printer data
    PrinterStatus _printer = {};

    // HMS errors
    static constexpr uint8_t MAX_HMS = 8;
    HmsError _hmsErrors[MAX_HMS] = {};
    uint8_t _hmsCount = 0;
    uint8_t _hmsViewIdx = 0;

    // Queue
    static constexpr uint8_t MAX_QUEUE = 5;
    char _queueJobs[MAX_QUEUE][64] = {};
    uint8_t _queueCount = 0;

    // Menu
    uint8_t _menuIdx = 0;
    static constexpr uint8_t MENU_ITEMS = 6;
    static const char* const MENU_LABELS[MENU_ITEMS];

    // Toast
    char _toastMsg[32] = {};
    uint32_t _toastExpiry = 0;

    // Rendering
    void _render();
    void _drawStatusBar();
    void _drawBoot();
    void _drawHome();
    void _drawMenu();
    void _drawPlateClear();
    void _drawHmsError();
    void _drawSpoolAssign();
    void _drawQueuePeek();
    void _drawSettings();
    void _drawProvisioning();
    void _drawError();
    void _drawToast();
};
