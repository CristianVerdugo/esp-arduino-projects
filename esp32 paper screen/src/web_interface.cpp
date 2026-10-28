#include "web_interface.h"

#include <WebServer.h>

#include "weather_time.h"

static WebServer server(80);

static void handleRoot() {
  char timeStr[24] = "not synced yet";
  getTimeString(timeStr, sizeof(timeStr), 100);
  const WeatherData &w = getWeather();

  String html =
      "<!DOCTYPE html><html><head><meta charset='utf-8'>"
      "<meta name='viewport' content='width=device-width,initial-scale=1'>"
      "<meta http-equiv='refresh' content='10'><title>Weather</title>"
      "<style>body{font-family:sans-serif;text-align:center;margin-top:40px}"
      "h1{font-size:3em;margin:0}</style></head><body>";
  html += "<h1>" + String(timeStr) + "</h1>";
  html += "<h2>" + w.placeName + " (" + getPostCode() + ")</h2>";
  if (w.valid) {
    html += "<p>" + String(weatherText(w.weatherCode)) + "</p>";
    html += "<p>Temperature: " + String(w.temperature, 1) + " &deg;C</p>";
    html += "<p>Humidity: " + String(w.humidity) + " %</p>";
    html += "<p>Wind: " + String(w.windSpeed, 1) + " km/h</p>";
  } else {
    html += "<p>Weather not loaded yet</p>";
  }
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void startWebInterface() {
  server.on("/", handleRoot);
  server.begin();
}

void handleWebInterface() { server.handleClient(); }
