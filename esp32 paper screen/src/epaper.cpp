#include "epaper.h"

#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>
#include <GxEPD2_BW.h>
#include <SPI.h>

#include "weather_time.h"

namespace {

// Partial refreshes slowly leave ghosting; a periodic full refresh (visible flash) clears it.
constexpr int PARTIAL_UPDATES_PER_FULL_REFRESH = 30;

constexpr int TIME_BASELINE_Y = 50;
constexpr int TIME_AREA_HEIGHT = 64;  // partial window covering only the clock
constexpr int SEPARATOR_Y = 70;
constexpr int LINES_BASELINE_Y = 98;
constexpr int LINE_STEP = 26;

GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT> display(
    GxEPD2_154_D67(EPD_PIN_CS, EPD_PIN_DC, EPD_PIN_RST, EPD_PIN_BUSY));

void drawCentered(const char* text, int baselineY) {
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((display.width() - w) / 2 - x1, baselineY);
  display.print(text);
}

void drawScreen(const char* time, const char* const* lines, size_t lineCount) {
  display.fillScreen(GxEPD_WHITE);
  display.setTextColor(GxEPD_BLACK);

  display.setFont(&FreeSansBold24pt7b);
  drawCentered(time, TIME_BASELINE_Y);

  display.fillRect(10, SEPARATOR_Y, display.width() - 20, 2, GxEPD_BLACK);

  display.setFont(&FreeSans9pt7b);
  for (size_t i = 0; i < lineCount; ++i) {
    drawCentered(lines[i], LINES_BASELINE_Y + i * LINE_STEP);
  }
}

}  // namespace

void epdUpdate() {
  static bool initialized = false;
  static unsigned long lastCheck = 0;
  static String lastTime;
  static String lastWeather;
  static int partialCount = 0;

  if (millis() - lastCheck < 1000) return;
  lastCheck = millis();

  const WeatherData& w = getWeather();
  char full[24];
  if (!w.valid || !getTimeString(full, sizeof(full), 100)) return;

  char hhmm[6];
  memcpy(hhmm, full + 11, 5);  // "YYYY-MM-DD HH:MM:SS" -> "HH:MM"
  hhmm[5] = '\0';

  String condition = weatherText(w.weatherCode);
  String temp = String("Temp: ") + String(w.temperature, 1) + " C";
  String humidity = String("Humidity: ") + w.humidity + "%";
  String wind = String("Wind: ") + String(w.windSpeed, 1) + " km/h";
  String weatherContent = condition + "|" + temp + "|" + humidity + "|" + wind;

  bool timeChanged = lastTime != hhmm;
  bool weatherChanged = lastWeather != weatherContent;
  if (!timeChanged && !weatherChanged) return;

  if (!initialized) {
    SPI.begin(EPD_PIN_CLK, -1, EPD_PIN_DIN, EPD_PIN_CS);
    display.init(0, true, 2, false);
    display.setRotation(0);
    initialized = true;
  }

  if (partialCount == 0) {
    display.setFullWindow();
  } else if (weatherChanged) {
    display.setPartialWindow(0, 0, display.width(), display.height());
  } else {
    display.setPartialWindow(0, 0, display.width(), TIME_AREA_HEIGHT);
  }

  const char* lines[] = {condition.c_str(), temp.c_str(), humidity.c_str(), wind.c_str()};
  display.firstPage();
  do {
    drawScreen(hhmm, lines, 4);
  } while (display.nextPage());
  display.powerOff();  // keeps controller RAM, needed for the next partial refresh

  partialCount = (partialCount + 1) % PARTIAL_UPDATES_PER_FULL_REFRESH;
  lastTime = hhmm;
  lastWeather = weatherContent;
}
