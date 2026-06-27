# BamBuddy Clear-Plate Button Box

ESP8266 D1 Mini based button box for clearing printer build plates in BamBuddy. Remove your print, hold ARM, dial the printer, push to confirm — BamBuddy starts the next queued job.

## Hardware

### Parts
- 1x Wemos D1 Mini (ESP8266)
- 1x Momentary push button (normally open) — ARM
- 1x KY-040 rotary encoder module — select printer + confirm
- 1x SSD1306 0.96" OLED display (128x64, I2C)
- Wire

### Wiring

**ARM button** (wire between D3 and GND — onboard pull-up):
| Function | Pin |
|----------|-----|
| ARM      | D3  |

**KY-040 Rotary Encoder:**
| Pin | D1 Mini Pin |
|-----|-------------|
| CLK | D5          |
| DT  | D6          |
| SW  | D7          |
| +   | 3.3V        |
| GND | GND         |

**OLED Display (I2C):**
| Pin | D1 Mini Pin |
|-----|-------------|
| SDA | D2          |
| SCL | D1          |
| VCC | 3.3V        |
| GND | GND         |

### OLED Display
Shows real-time status with a printer selector UI:
- **Boot:** "BamBuddy / Button Box"
- **Disarmed:** "DISARMED / Hold ARM to enable"
- **Armed:** Printer number with ◀/▶ arrows and "push to clear" hint
- **Clearing:** "Printer 3 / Clearing..."
- **Success:** "Printer 3 / Cleared!"
- **Error:** "Printer 3 / FAILED"

## Setup

### 1. Install PlatformIO
```bash
# VS Code extension (recommended)
# or CLI:
pip install platformio
```

### 2. Configure
Edit `include/config.h`:
```cpp
#define WIFI_SSID     "your-wifi"
#define WIFI_PASSWORD "your-password"
#define BAMBUDDY_URL  "http://192.168.1.100:8000"
#define API_KEY       "your-api-key"
```

### 3. Create an API Key in BamBuddy
1. Go to **Settings → API Keys**
2. Click **Create API Key**
3. Enable the **Can Control Printer** and **Can Read Status** scopes
4. Copy the key into `config.h`

### 4. Build & Flash
```bash
cd hardware/esp32-clear-plate
pio run -t upload
```

### 5. Monitor Serial Output
```bash
pio device monitor
```

## How It Works
1. On boot, fetches the printer list from BamBuddy (`GET /api/v1/printers/`)
2. Hold the **ARM button** (dead man's switch) — display shows printer selector with name
3. **Rotate** the encoder knob to pick a printer (wraps around, shows name + position)
4. **Push** the encoder knob to confirm — sends `POST /api/v1/printers/{id}/clear-plate`
5. Display shows "Cleared!" or "FAILED" for 2 seconds
6. Release ARM to disarm

## Customization

### Printer list
Printers are fetched automatically at startup — no configuration needed. If you add or remove printers in BamBuddy, just reboot the device.

### No auth
If BamBuddy auth is disabled, the `X-API-Key` header is ignored — the firmware works as-is.
