#include "epaper.h"

#include <Adafruit_GFX.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>
#include <GxEPD2_BW.h>
#include <SPI.h>

#include "weather_time.h"

namespace {

// Full refresh (slow, flashes black) clears ghosting; done at startup and once a night only.
constexpr const char* FULL_REFRESH_TIME = "03:00";

constexpr int TIME_BASELINE_Y = 44;
constexpr int DATE_BASELINE_Y = 62;
constexpr int SEPARATOR_Y = 70;
constexpr int CONDITION_BASELINE_Y = 94;
constexpr int TEMP_BASELINE_Y = 136;
constexpr int HUMIDITY_BASELINE_Y = 178;

// Panel glass variant; the wrong one still does full refreshes but gives gray/blurry partial updates.
//   GxEPD2_154_GDEY0154D67 : newer Waveshare 1.54" V2 (FPC label "FPC-B001")
//   GxEPD2_154_D67         : older Waveshare 1.54" V2 (FPC label "HINK-E154A07-A1")
//   GxEPD2_154             : Waveshare 1.54" V1 (no "V2" sticker, FPC "WFC0000CZ07")
using EpdDriver = GxEPD2_154_GDEY0154D67;

GxEPD2_BW<EpdDriver, EpdDriver::HEIGHT> display(
    EpdDriver(EPD_PIN_CS, EPD_PIN_DC, EPD_PIN_RST, EPD_PIN_BUSY));

// Whole screen image; bit 1 = white, same layout the controller RAM expects.
GFXcanvas1 canvas(EpdDriver::WIDTH, EpdDriver::HEIGHT);

void drawCentered(const char* text, int baselineY) {
  int16_t x1, y1;
  uint16_t w, h;
  canvas.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  canvas.setCursor((canvas.width() - w) / 2 - x1, baselineY);
  canvas.print(text);
}

constexpr int ICON_WIDTH = 20;
constexpr int ICON_HEIGHT = 28;
constexpr int ICON_GAP = 6;

void drawThermometer(int x, int top) {
  canvas.drawRoundRect(x + 6, top, 8, 20, 4, GxEPD_BLACK);
  canvas.fillRect(x + 8, top + 8, 4, 14, GxEPD_BLACK);
  canvas.fillCircle(x + 10, top + 22, 6, GxEPD_BLACK);
}

void drawDroplet(int x, int top) {
  canvas.fillTriangle(x + 10, top, x + 2, top + 15, x + 18, top + 15, GxEPD_BLACK);
  canvas.fillCircle(x + 10, top + 19, 9, GxEPD_BLACK);
}

// Draws icon followed by text, centered as one group.
void drawIconLine(void (*drawIcon)(int, int), const char* text, int baselineY) {
  int16_t x1, y1;
  uint16_t w, h;
  canvas.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  int startX = (canvas.width() - (ICON_WIDTH + ICON_GAP + w)) / 2;
  drawIcon(startX, baselineY - ICON_HEIGHT);
  canvas.setCursor(startX + ICON_WIDTH + ICON_GAP - x1, baselineY);
  canvas.print(text);
}

void drawScreen(const char* time, const char* date, const char* condition, const char* temp,
                const char* humidity) {
  canvas.fillScreen(GxEPD_WHITE);
  canvas.setTextColor(GxEPD_BLACK);

  canvas.setFont(&FreeSansBold24pt7b);
  drawCentered(time, TIME_BASELINE_Y);

  canvas.setFont(&FreeSans9pt7b);
  drawCentered(date, DATE_BASELINE_Y);

  canvas.fillRect(10, SEPARATOR_Y, canvas.width() - 20, 2, GxEPD_BLACK);

  canvas.setFont(&FreeSans9pt7b);
  drawCentered(condition, CONDITION_BASELINE_Y);

  canvas.setFont(&FreeSansBold18pt7b);
  drawIconLine(drawThermometer, temp, TEMP_BASELINE_Y);
  drawIconLine(drawDroplet, humidity, HUMIDITY_BASELINE_Y);
}

}  // namespace

void epdUpdate() {
  static bool initialized = false;
  static unsigned long lastCheck = 0;
  static String lastTime;
  static String lastWeather;

  if (millis() - lastCheck < 1000) return;
  lastCheck = millis();

  const WeatherData& w = getWeather();
  char full[24];
  if (!w.valid || !getTimeString(full, sizeof(full), 100)) return;

  char hhmm[6];
  memcpy(hhmm, full + 11, 5);  // "YYYY-MM-DD HH:MM:SS" -> "HH:MM"
  hhmm[5] = '\0';

  // "YYYY-MM-DD" -> "d:m:YYYY"
  char date[12];
  snprintf(date, sizeof(date), "%d:%d:%.4s", atoi(full + 8), atoi(full + 5), full);

  String condition = weatherText(w.weatherCode);
  String temp = String(": ") + String(w.temperature, 1) + " C";
  String humidity = String(": ") + w.humidity + "%";
  String weatherContent = condition + "|" + temp + "|" + humidity;

  bool timeChanged = lastTime != hhmm;
  bool weatherChanged = lastWeather != weatherContent;
  if (!timeChanged && !weatherChanged) return;

  if (!initialized) {
    SPI.begin(EPD_PIN_CLK, -1, EPD_PIN_DIN, EPD_PIN_CS);
    display.init(0, true, 2, false);
  }

  drawScreen(hhmm, date, condition.c_str(), temp.c_str(), humidity.c_str());
  const uint8_t* image = canvas.getBuffer();
  const int16_t W = EpdDriver::WIDTH;
  const int16_t H = EpdDriver::HEIGHT;

  if (!initialized || (timeChanged && strcmp(hhmm, FULL_REFRESH_TIME) == 0)) {
    display.epd2.writeImageForFullRefresh(image, 0, 0, W, H);
    display.epd2.refresh(false);
  } else {
    // A differential refresh leaves unchanged pixels undriven and they fade to gray.
    // Claiming the previous image was the inverse makes the fast waveform re-drive every pixel.
    display.epd2.writeImage(image, 0, 0, W, H);
    display.epd2.writeImageToPrevious(image, 0, 0, W, H, true);
    display.epd2.refresh(true);
  }
  display.epd2.writeImageAgain(image, 0, 0, W, H);
  display.epd2.powerOff();

  initialized = true;
  lastTime = hhmm;
  lastWeather = weatherContent;
}
