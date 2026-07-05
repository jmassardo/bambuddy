#pragma once
// SpoolBuddy v3 — Pin Definitions
// ESP32-S3-DevKitC-1 + ST7796S 4" TFT + FT6336U Touch + PN5180 + HX711
//
// Module: Hosyond 4.0" 320x480 TN Capacitive Touch (14-pin header)
//   LCD Driver: ST7796S (4-line SPI)
//   Touch Driver: FT6336U (I2C, addr 0x38)
//   Power: 5V recommended, 0.5W

#include <Arduino.h>

// ─── ST7796S TFT Display (SPI2 / FSPI) ─────────────────────────────
#define PIN_TFT_CS      7     // Chip select
#define PIN_TFT_DC      3     // Data/Command (RS)
#define PIN_TFT_RST     4     // Hardware reset
#define PIN_TFT_MOSI    6     // SPI data (SDI on module)
#define PIN_TFT_SCK     5     // SPI clock
#define PIN_TFT_MISO   -1     // Not connected (write-only)
#define PIN_TFT_BL     14     // Backlight LED (PWM, active HIGH)

// ─── FT6336U Capacitive Touch (I2C) ────────────────────────────────
#define PIN_TOUCH_SDA   15    // I2C data
#define PIN_TOUCH_SCL   16    // I2C clock
#define PIN_TOUCH_INT   17    // Interrupt (active LOW)
#define PIN_TOUCH_RST   -1    // Tied to TFT_RST (shared reset)
#define TOUCH_I2C_ADDR  0x38  // FT6336U default address

// ─── PN5180 NFC Reader (SPI3 / HSPI) ───────────────────────────────
#define PIN_NFC_CS      10    // NSS / chip select
#define PIN_NFC_MOSI    11    // SPI data out
#define PIN_NFC_SCK     12    // SPI clock
#define PIN_NFC_MISO    13    // SPI data in
#define PIN_NFC_BUSY     9    // Busy signal
#define PIN_NFC_RST      8    // Hardware reset

// ─── HX711 Load Cell ────────────────────────────────────────────────
#define PIN_HX711_DOUT   1    // Data out
#define PIN_HX711_SCK    2    // Clock

// ─── Misc ───────────────────────────────────────────────────────────
#define PIN_BUZZER      21    // Piezo buzzer (PWM)
#define PIN_RGB_LED     48    // Built-in DevKit RGB NeoPixel

// ─── SPI Bus Assignments ────────────────────────────────────────────
// SPI2 (FSPI): TFT display — high bandwidth, 40-80 MHz
// SPI3 (HSPI): PN5180 NFC — lower bandwidth, 7 MHz
// Both buses operate independently (no contention)
