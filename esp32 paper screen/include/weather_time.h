#pragma once

#include <Arduino.h>

struct WeatherData {
  bool valid = false;
  String placeName = "?";
  float temperature = 0;
  int humidity = 0;
  float windSpeed = 0;
  int weatherCode = 0;
};

// Looks up the location, refreshes weather periodically and prints the time; call from loop().
void updateWeatherAndTime();

const WeatherData &getWeather();
const char *weatherText(int code);
const char *getPostCode();

// Writes the local time to buf; returns false if the clock is not synced yet.
bool getTimeString(char *buf, size_t len, uint32_t timeoutMs = 100);
