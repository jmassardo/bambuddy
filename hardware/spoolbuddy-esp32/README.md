# SpoolBuddy ESP32 Firmware

ESP32-S3 firmware for SpoolBuddy — reads NFC tags (PN5180) and scale weight
(HX711), reports events to the Bambuddy backend via WiFi/HTTP.

## Hardware

| Component | Interface | Notes |
|-----------|-----------|-------|
| ESP32-S3-DevKitC-1 | — | Dual-core 240 MHz, WiFi, 8 MB PSRAM |
| PN5180 NFC reader | SPI (500 kHz) | MIFARE Classic + NTAG read/write |
| HX711 load cell amp | 2-wire GPIO | 24-bit ADC, 10/80 SPS |
| SSD1306 OLED (optional) | I2C | 128×64 status display |

## Wiring

### PN5180 → ESP32-S3

| PN5180 | GPIO | Color |
|--------|------|-------|
| 3V3 | 3V3 | Red |
| 5V | 5V (VBUS) | Red |
| GND | GND | Black |
| SCK | 12 | Yellow |
| MISO | 13 | Blue |
| MOSI | 11 | Green |
| NSS | 10 | Orange |
| BUSY | 9 | White |
| RST | 8 | Brown |

### HX711 → ESP32-S3

| HX711 | GPIO | Color |
|-------|------|-------|
| VCC | 3V3 | Red |
| GND | GND | Black |
| SCK | 2 | Yellow |
| DOUT | 1 | White |

## Building

Requires [PlatformIO](https://platformio.org/).

```bash
# Build
pio run

# Upload
pio run --target upload

# Monitor serial
pio device monitor
```

## First-Run Configuration

On first boot (no stored config), the ESP32 starts a WiFi AP named
`SpoolBuddy-Setup`. Connect to it and configure via the captive portal.

**Serial fallback** (development):

```
wifi:YourSSID,YourPassword
backend:http://192.168.1.100:5000,your-api-key
boot
```

## OTA Updates

Firmware updates are delivered over HTTP. The Bambuddy backend sends a
`firmware_url` in the heartbeat response when an update is available.
The ESP32 downloads and flashes the new firmware with automatic rollback
if the first heartbeat after update fails.

## Architecture

```
Core 0: NFC poll task (300ms) + Scale read task (100ms)
Core 1: Heartbeat task (10s) + WiFi stack
```

NFC and scale run on Core 0 to avoid WiFi interrupt interference with
timing-sensitive SPI/GPIO operations. HTTP communication runs on Core 1
alongside the WiFi stack.
