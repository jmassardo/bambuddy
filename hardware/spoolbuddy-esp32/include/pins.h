#pragma once

// PN5180 NFC Reader (SPI)
#define PIN_NFC_SCK   12
#define PIN_NFC_MISO  13
#define PIN_NFC_MOSI  11
#define PIN_NFC_NSS   10  // Manual chip select
#define PIN_NFC_BUSY   9  // Input — wait for HIGH→LOW
#define PIN_NFC_RST    8  // Output — active HIGH

// HX711 Load Cell Amplifier (bit-bang 2-wire)
#define PIN_HX711_SCK  2
#define PIN_HX711_DOUT 1

// Optional SSD1306 OLED (I2C)
#define PIN_OLED_SDA  3
#define PIN_OLED_SCL  4

// Built-in RGB LED (ESP32-S3-DevKitC-1)
#define PIN_LED_RGB   48
