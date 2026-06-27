#pragma once

// ─── WiFi ───────────────────────────────────────────────────────────────────
#define WIFI_SSID     "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

// ─── BamBuddy Server ────────────────────────────────────────────────────────
// Base URL of your BamBuddy instance (no trailing slash)
#define BAMBUDDY_URL  "http://192.168.1.100:8000"

// API key — needs "can_control_printer" and "can_read_status" scopes
// Generate one in BamBuddy: Settings → API Keys → Create
#define API_KEY       "your-api-key-here"

// ─── GPIO Pin Mapping (D1 Mini labels → GPIO numbers) ──────────────────────
// ARM button: wire between pin and GND (has onboard pull-up)
#define ARM_PIN        D3  // GPIO0 — onboard pull-up, safe for boot

// KY-040 Rotary Encoder
#define ENC_CLK_PIN    D5  // GPIO14
#define ENC_DT_PIN     D6  // GPIO12
#define ENC_SW_PIN     D7  // GPIO13

// ─── OLED Display (SSD1306 128x64, I2C) ─────────────────────────────────────
// Uses default I2C pins on D1 Mini
#define OLED_SDA       D2  // GPIO4
#define OLED_SCL       D1  // GPIO5
#define OLED_ADDRESS   0x3C

// ─── Printer Configuration ──────────────────────────────────────────────────
// Max printers the device can hold (memory limit). The actual list is
// fetched from BamBuddy at startup via GET /api/v1/printers/.
#define MAX_PRINTERS   32

// ─── Timing ─────────────────────────────────────────────────────────────────
#define DEBOUNCE_MS       50    // Button debounce time
#define HTTP_TIMEOUT_MS   5000  // HTTP request timeout
#define WIFI_RETRY_MS     500   // Delay between WiFi reconnect attempts
