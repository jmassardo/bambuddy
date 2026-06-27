#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>
#include <U8g2lib.h>
#include <Wire.h>
#include "config.h"

// ─── Display ────────────────────────────────────────────────────────────────

U8G2_SSD1306_128X64_NONAME_F_HW_I2C display(U8G2_R0, U8X8_PIN_NONE);

void displayCentered(const char* line1, const char* line2 = nullptr,
                     const uint8_t* font1 = u8g2_font_helvB14_tr,
                     const uint8_t* font2 = u8g2_font_helvR12_tr) {
    display.clearBuffer();
    display.setFont(font1);
    int w1 = display.getStrWidth(line1);
    if (line2) {
        display.drawStr((128 - w1) / 2, 28, line1);
        display.setFont(font2);
        int w2 = display.getStrWidth(line2);
        display.drawStr((128 - w2) / 2, 54, line2);
    } else {
        display.drawStr((128 - w1) / 2, 40, line1);
    }
    display.sendBuffer();
}

void displaySelector(int index) {
    const char* name = printers[index].name;

    display.clearBuffer();

    // Printer name at top (auto-size to fit)
    display.setFont(u8g2_font_helvR10_tr);
    int wn = display.getStrWidth(name);
    if (wn > 120) {
        display.setFont(u8g2_font_helvR08_tr);
        wn = display.getStrWidth(name);
    }
    display.drawStr((128 - wn) / 2, 14, name);

    // Big printer number in the center
    char num[8];
    snprintf(num, sizeof(num), "%d/%d", index + 1, printerCount);
    display.setFont(u8g2_font_helvB18_tr);
    int wc = display.getStrWidth(num);
    display.drawStr((128 - wc) / 2, 46, num);

    display.setFont(u8g2_font_open_iconic_arrow_2x_t);
    display.drawGlyph(4, 42, 0x0044);
    display.drawGlyph(108, 42, 0x0042);

    display.setFont(u8g2_font_helvR08_tr);
    const char* hint = "push to clear";
    int wh = display.getStrWidth(hint);
    display.drawStr((128 - wh) / 2, 63, hint);

    display.sendBuffer();
}

// ─── Rotary Encoder (interrupt-driven) ──────────────────────────────────────

volatile int encoderDelta = 0;
volatile bool lastCLK = HIGH;

void IRAM_ATTR encoderISR() {
    bool clk = digitalRead(ENC_CLK_PIN);
    if (clk != lastCLK && clk == LOW) {
        if (digitalRead(ENC_DT_PIN) != clk) {
            encoderDelta++;
        } else {
            encoderDelta--;
        }
    }
    lastCLK = clk;
}

// ─── Debounced Button ───────────────────────────────────────────────────────

struct DebouncedButton {
    int pin;
    bool lastRaw;
    bool state;
    bool pressed;
    unsigned long debounceTime;

    void init(int p) {
        pin = p;
        pinMode(pin, INPUT_PULLUP);
        lastRaw = HIGH;
        state = HIGH;
        pressed = false;
        debounceTime = 0;
    }

    void update(unsigned long now) {
        bool raw = digitalRead(pin);
        pressed = false;

        if (raw != lastRaw) {
            debounceTime = now;
        }

        if ((now - debounceTime) > DEBOUNCE_MS) {
            if (raw != state) {
                bool wasHigh = (state == HIGH);
                state = raw;
                if (wasHigh && state == LOW) {
                    pressed = true;
                }
            }
        }
        lastRaw = raw;
    }

    bool isHeld() { return state == LOW; }
};

DebouncedButton armBtn, encBtn;

// ─── WiFi ───────────────────────────────────────────────────────────────────

void connectWiFi() {
    if (WiFi.status() == WL_CONNECTED) return;

    Serial.printf("Connecting to WiFi: %s", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    displayCentered("Connecting...", WIFI_SSID);

    while (WiFi.status() != WL_CONNECTED) {
        delay(WIFI_RETRY_MS);
        Serial.print(".");
    }

    Serial.printf("\nConnected! IP: %s\n", WiFi.localIP().toString().c_str());
}

// ─── Printer List (fetched from BamBuddy at startup) ────────────────────────

struct PrinterInfo {
    int id;
    char name[24]; // truncated to fit OLED
};

PrinterInfo printers[MAX_PRINTERS];
int printerCount = 0;

bool fetchPrinters() {
    if (WiFi.status() != WL_CONNECTED) {
        connectWiFi();
    }

    WiFiClient client;
    HTTPClient http;
    String url = String(BAMBUDDY_URL) + "/api/v1/printers/";

    http.begin(client, url);
    http.setTimeout(HTTP_TIMEOUT_MS);
    http.addHeader("X-API-Key", API_KEY);

    Serial.printf("GET %s\n", url.c_str());
    int httpCode = http.GET();

    if (httpCode != 200) {
        Serial.printf("  ERR fetching printers: HTTP %d\n", httpCode);
        http.end();
        return false;
    }

    String response = http.getString();
    http.end();

    JsonDocument doc;
    if (deserializeJson(doc, response) != DeserializationError::Ok) {
        Serial.println("  ERR parsing printer list");
        return false;
    }

    JsonArray arr = doc.as<JsonArray>();
    printerCount = 0;
    for (JsonObject obj : arr) {
        if (printerCount >= MAX_PRINTERS) break;
        printers[printerCount].id = obj["id"] | 0;
        const char* name = obj["name"] | "Unknown";
        strncpy(printers[printerCount].name, name,
                sizeof(printers[0].name) - 1);
        printers[printerCount].name[sizeof(printers[0].name) - 1] = '\0';
        Serial.printf("  Printer %d: %s\n",
                      printers[printerCount].id,
                      printers[printerCount].name);
        printerCount++;
    }

    Serial.printf("Loaded %d printers\n", printerCount);
    return printerCount > 0;
}

// ─── Clear Plate Request ────────────────────────────────────────────────────

bool clearPlate(int printerId) {
    if (WiFi.status() != WL_CONNECTED) {
        connectWiFi();
    }

    WiFiClient client;
    HTTPClient http;
    String url = String(BAMBUDDY_URL) + "/api/v1/printers/" +
                 String(printerId) + "/clear-plate";

    http.begin(client, url);
    http.setTimeout(HTTP_TIMEOUT_MS);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-API-Key", API_KEY);

    Serial.printf("POST %s\n", url.c_str());
    int httpCode = http.POST("");

    bool success = false;
    if (httpCode == 200) {
        String response = http.getString();
        JsonDocument doc;
        if (deserializeJson(doc, response) == DeserializationError::Ok) {
            success = doc["success"] | false;
            const char* msg = doc["message"] | "no message";
            Serial.printf("  OK  Printer %d: %s\n", printerId, msg);
        }
    } else {
        String body = http.getString();
        Serial.printf("  ERR Printer %d: HTTP %d - %s\n",
                      printerId, httpCode, body.c_str());
    }

    http.end();
    return success;
}

// ─── State ──────────────────────────────────────────────────────────────────

int selectedIndex = 0;
unsigned long messageExpiry = 0;

// ─── Setup ──────────────────────────────────────────────────────────────────

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n=== BamBuddy Clear-Plate Button Box ===\n");

    Wire.begin(OLED_SDA, OLED_SCL);
    display.setI2CAddress(OLED_ADDRESS * 2);
    display.begin();
    displayCentered("BamBuddy", "Button Box");
    delay(1500);

    armBtn.init(ARM_PIN);
    encBtn.init(ENC_SW_PIN);

    pinMode(ENC_CLK_PIN, INPUT_PULLUP);
    pinMode(ENC_DT_PIN, INPUT_PULLUP);
    lastCLK = digitalRead(ENC_CLK_PIN);
    attachInterrupt(digitalPinToInterrupt(ENC_CLK_PIN), encoderISR, CHANGE);

    connectWiFi();

    displayCentered("Loading...", "Fetching printers");
    if (!fetchPrinters()) {
        displayCentered("ERROR", "No printers found");
        Serial.println("FATAL: Could not load printers from BamBuddy");
        while (true) { delay(1000); }
    }

    displayCentered("DISARMED", "Hold ARM to enable",
                    u8g2_font_helvB14_tr, u8g2_font_helvR10_tr);
    Serial.println("Ready. Hold ARM, dial printer, push to clear.\n");
}

// ─── Loop ───────────────────────────────────────────────────────────────────

void loop() {
    unsigned long now = millis();

    armBtn.update(now);
    encBtn.update(now);

    bool armed = armBtn.isHeld();

    static bool wasArmed = false;
    if (armed && !wasArmed) {
        messageExpiry = 0;
        displaySelector(selectedIndex);
        Serial.println("ARMED");
    } else if (!armed && wasArmed) {
        messageExpiry = 0;
        displayCentered("DISARMED", "Hold ARM to enable",
                        u8g2_font_helvB14_tr, u8g2_font_helvR10_tr);
        Serial.println("Disarmed");
    }
    wasArmed = armed;

    if (!armed) {
        delay(1);
        return;
    }

    if (messageExpiry > 0 && now > messageExpiry) {
        messageExpiry = 0;
        displaySelector(selectedIndex);
    }

    if (messageExpiry == 0) {
        noInterrupts();
        int delta = encoderDelta;
        encoderDelta = 0;
        interrupts();

        if (delta != 0) {
            selectedIndex = (selectedIndex + delta % printerCount + printerCount)
                            % printerCount;
            displaySelector(selectedIndex);
            Serial.printf("Selected: %s (id=%d)\n",
                          printers[selectedIndex].name,
                          printers[selectedIndex].id);
        }
    }

    if (encBtn.pressed) {
        int pid = printers[selectedIndex].id;
        const char* name = printers[selectedIndex].name;

        displayCentered(name, "Clearing...");
        Serial.printf("Clearing plate for %s (id=%d)\n", name, pid);

        bool ok = clearPlate(pid);

        if (ok) {
            displayCentered(name, "Cleared!");
        } else {
            displayCentered(name, "FAILED");
        }

        messageExpiry = millis() + 2000;
    }

    delay(1);
}
