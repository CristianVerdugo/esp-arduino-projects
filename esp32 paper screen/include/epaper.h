#pragma once

#include <Arduino.h>

// Waveshare 1.54" e-Paper module V2 (SSD1681, 200x200, black/white)
// Wiring (Seeed XIAO ESP32C3)
constexpr uint8_t EPD_PIN_DIN = D10;
constexpr uint8_t EPD_PIN_CLK = D8;
constexpr uint8_t EPD_PIN_CS = D1;
constexpr uint8_t EPD_PIN_DC = D2;
constexpr uint8_t EPD_PIN_RST = D3;
constexpr uint8_t EPD_PIN_BUSY = D4;

// Call from loop(): shows the time (HH:MM) and weather, redrawing only what changed.
void epdUpdate();
