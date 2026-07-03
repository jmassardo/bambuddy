#pragma once

// ---------------------------------------------------------
// BamBuddy Printer Panel — ESP32-S3 Super Mini / Zero
// Hardware: 1" SSD1306 OLED (I2C), rotary encoder, 2 buttons
// ---------------------------------------------------------

// OLED Display (I2C SSD1306 128x64)
#define PIN_OLED_SDA   8
#define PIN_OLED_SCL   9

// Rotary Encoder
#define PIN_ENC_A      5   // CLK / A phase
#define PIN_ENC_B      6   // DT  / B phase
#define PIN_ENC_SW     7   // Push-button (active LOW)

// Control Buttons (active LOW, use internal pull-up)
#define PIN_BTN_BACK   3   // BACK button
#define PIN_BTN_CLR    4   // CLR (quick plate-clear) button

// Onboard LED (active LOW on most S3 Mini clones)
#define PIN_LED        47
