#include <Arduino.h>

#include "epaper.h"
#include "weather_time.h"
#include "web_interface.h"
#include "wifi_manager.h"

void setup() {
  Serial.begin(115200);
  delay(1000);

  loadSavedLocation();
  connectWiFi();
  startWebInterface();
}

void loop() {
  handleWebInterface();

  if (!ensureWiFi()) return;

  updateWeatherAndTime();
  epdUpdate();
  delay(10);
}
