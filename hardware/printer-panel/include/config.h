#pragma once

// ---------------------------------------------------------
// BamBuddy Printer Panel — Configuration
// These defaults are overridden by NVS after provisioning
// ---------------------------------------------------------

// BamBuddy server connection
#define DEFAULT_SERVER_HOST  "nuc1.lab.dxrf.com"
#define DEFAULT_SERVER_PORT  8000
#define DEFAULT_API_KEY      ""  // Set via provisioning

// Device identity
#define DEVICE_TYPE          "printer-panel"
#define FIRMWARE_VERSION     "0.1.0"

// Timing (ms)
#define HEARTBEAT_INTERVAL   10000   // 10s heartbeat to server
#define STATUS_POLL_INTERVAL 5000    // 5s printer status poll
#define DISPLAY_TIMEOUT_MS   60000   // 1min screen blank
#define DEBOUNCE_MS          50      // Button debounce
#define ENCODER_DEBOUNCE_MS  5       // Encoder state debounce

// Display
#define DISPLAY_WIDTH        128
#define DISPLAY_HEIGHT       64
#define STATUS_BAR_HEIGHT    10
#define CONTENT_Y_START      12

// Network
#define WIFI_CONNECT_TIMEOUT 15000   // 15s before giving up
#define HTTP_TIMEOUT         5000    // 5s API call timeout
