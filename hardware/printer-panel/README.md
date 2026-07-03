# BamBuddy Printer Panel

Per-printer control panel using an ESP32-S3 Super Mini / Zero with a 1" OLED display, rotary encoder, and two buttons.

## Hardware

| Component | Spec |
|-----------|------|
| MCU | ESP32-S3 Super Mini (or S3 Zero) |
| Display | 0.96"/1.3" SSD1306 OLED 128x64 (I2C) |
| Encoder | KY-040 rotary encoder with push button |
| Buttons | 2x tactile (BACK, CLR) |
| Mount | Perfboard, one per printer |

### Pin Assignments (see `include/pins.h`)

| Function | GPIO |
|----------|------|
| OLED SDA | 8 |
| OLED SCL | 9 |
| Encoder A | 5 |
| Encoder B | 6 |
| Encoder SW | 7 |
| BACK button | 3 |
| CLR button | 4 |

## Building

```bash
# Install PlatformIO CLI
pip install platformio

# Build
cd hardware/printer-panel
pio run

# Upload (with board connected via USB)
pio run -t upload

# Monitor serial output
pio device monitor
```

## Configuration

Edit the `#define` block at the top of `src/main.cpp` for your setup:

```cpp
#define WIFI_SSID      "YOUR_SSID"
#define WIFI_PASS      "YOUR_PASSWORD"
#define DEVICE_ID      "panel-001"
#define PRINTER_ID     3
#define API_KEY        "YOUR_API_KEY"
```

Future: captive portal provisioning (Issue #5 Sprint 4).

## Features

- **Home screen**: Live printer status (idle/printing/done/failed)
- **Plate clear**: CLR button shortcut (press=confirm, long-press=immediate)
- **HMS errors**: Auto-popup on error, scroll through, acknowledge
- **Queue peek**: View next pending jobs
- **Spool assign**: (planned) AMS slot management
- **OTA updates**: Firmware delivered via heartbeat response

## Architecture

```
main.cpp            → setup/loop, event routing
├── display.h/cpp   → OLED rendering, screen state machine
├── input.h/cpp     → encoder ISR + button debounce → InputEvent queue
└── api_client.h/cpp → WiFi, heartbeat, BamBuddy API client
```

The panel communicates with BamBuddy via the fleet device API:
- `POST /api/v1/spoolbuddy/register` — one-time registration
- `POST /api/v1/spoolbuddy/{id}/heartbeat` — periodic status + receive commands
- `GET /api/v1/spoolbuddy/devices/{id}/printer-status` — live printer state
- `POST /api/v1/spoolbuddy/devices/{id}/plate-clear` — clear build plate
- `GET /api/v1/spoolbuddy/devices/{id}/queue` — pending print jobs
- `POST /api/v1/spoolbuddy/devices/{id}/hms-ack` — acknowledge HMS error

## UI Simulator

Open `docs/ui-simulator.html` in a browser to preview the screen layout and navigation without hardware.
