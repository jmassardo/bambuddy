#pragma once
// LovyanGFX configuration for Hosyond 4.0" ST7796S + FT6336U
// ESP32-S3, SPI2, no PSRAM

#include <LovyanGFX.hpp>
#include "pins_v3.h"

class LGFX : public lgfx::LGFX_Device {
public:
    lgfx::Panel_ST7796  _panel_instance;
    lgfx::Bus_SPI       _bus_instance;
    lgfx::Light_PWM     _light_instance;
    lgfx::Touch_FT5x06  _touch_instance;  // FT6336U is FT5x06 family

    LGFX(void) {
        // ── SPI Bus Configuration ──────────────────────────────
        {
            auto cfg = _bus_instance.config();

            cfg.spi_host   = SPI2_HOST;     // FSPI
            cfg.spi_mode   = 0;
            cfg.freq_write = 40000000;      // 40 MHz write (can try 80M)
            cfg.freq_read  = 16000000;      // 16 MHz read
            cfg.pin_sclk   = PIN_TFT_SCK;
            cfg.pin_mosi   = PIN_TFT_MOSI;
            cfg.pin_miso   = PIN_TFT_MISO;  // -1 = not connected
            cfg.pin_dc     = PIN_TFT_DC;
            cfg.spi_3wire  = false;
            cfg.use_lock   = true;
            cfg.dma_channel = SPI_DMA_CH_AUTO;

            _bus_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }

        // ── Panel Configuration ────────────────────────────────
        {
            auto cfg = _panel_instance.config();

            cfg.pin_cs   = PIN_TFT_CS;
            cfg.pin_rst  = PIN_TFT_RST;
            cfg.pin_busy = -1;

            cfg.panel_width  = 320;
            cfg.panel_height = 480;
            cfg.offset_x     = 0;
            cfg.offset_y     = 0;
            cfg.offset_rotation = 0;

            cfg.readable   = false;   // MISO not connected
            cfg.invert     = false;   // TN panel, no inversion needed
            cfg.rgb_order  = false;   // BGR order (ST7796S default)
            cfg.dlen_16bit = false;   // 8-bit SPI transfers
            cfg.bus_shared = false;   // Dedicated SPI bus (not shared with touch)

            _panel_instance.config(cfg);
        }

        // ── Backlight Configuration ────────────────────────────
        {
            auto cfg = _light_instance.config();

            cfg.pin_bl = PIN_TFT_BL;
            cfg.invert = false;       // HIGH = brighter
            cfg.freq   = 12000;       // 12 kHz PWM (silent)
            cfg.pwm_channel = 0;

            _light_instance.config(cfg);
            _panel_instance.setLight(&_light_instance);
        }

        // ── Touch Configuration (FT6336U) ──────────────────────
        {
            auto cfg = _touch_instance.config();

            cfg.i2c_port = 0;              // I2C port 0
            cfg.i2c_addr = TOUCH_I2C_ADDR; // 0x38
            cfg.pin_sda  = PIN_TOUCH_SDA;
            cfg.pin_scl  = PIN_TOUCH_SCL;
            cfg.pin_int  = PIN_TOUCH_INT;
            cfg.pin_rst  = PIN_TOUCH_RST;  // -1 (shared with TFT RST)
            cfg.freq     = 400000;         // 400 kHz I2C
            cfg.x_min    = 0;
            cfg.x_max    = 319;
            cfg.y_min    = 0;
            cfg.y_max    = 479;
            cfg.bus_shared = false;

            _touch_instance.config(cfg);
            _panel_instance.setTouch(&_touch_instance);
        }

        setPanel(&_panel_instance);
    }
};
