#include "web_interface.h"

#include <WebServer.h>

#include "weather_time.h"

static WebServer server(80);

static String infoHtml() {
  char timeStr[24] = "not synced yet";
  getTimeString(timeStr, sizeof(timeStr), 100);
  const WeatherData &w = getWeather();

  String html = "<h1>" + String(timeStr) + "</h1>";
  html += "<h2>" + w.placeName + " (" + getCountryCode() + " " + getPostCode() + ")</h2>";
  if (w.valid) {
    html += "<p>" + String(weatherText(w.weatherCode)) + "</p>";
    html += "<p>Temperature: " + String(w.temperature, 1) + " &deg;C</p>";
    html += "<p>Humidity: " + String(w.humidity) + " %</p>";
    html += "<p>Wind: " + String(w.windSpeed, 1) + " km/h</p>";
  } else {
    html += "<p>Weather not loaded yet</p>";
  }
  return html;
}

static void handleInfo() { server.send(200, "text/html", infoHtml()); }

static void handleRoot() {
  String html =
      "<!DOCTYPE html><html><head><meta charset='utf-8'>"
      "<meta name='viewport' content='width=device-width,initial-scale=1'>"
      "<title>Weather</title>"
      "<style>body{font-family:sans-serif;text-align:center;margin-top:40px}"
      "h1{font-size:3em;margin:0}input{padding:6px;margin:4px}</style></head><body>"
      "<div id='info'>";
  html += infoHtml();
  html += "</div><hr><h3>Change location</h3>";
  String msg = server.arg("msg");
  if (msg == "ok") html += "<p>Location updated.</p>";
  else if (msg == "err") html += "<p>Location not found or invalid.</p>";
  html +=
      "<form method='POST' action='/location'>"
      "<input name='country' maxlength='2' size='3' placeholder='de' required> "
      "<input name='postcode' maxlength='12' size='10' placeholder='Post code' required> "
      "<button type='submit'>Set</button></form>"
      "<script>setInterval(function(){fetch('/info').then(function(r){return r.text()})"
      ".then(function(t){document.getElementById('info').innerHTML=t})},10000);</script>"
      "</body></html>";
  server.send(200, "text/html", html);
}

static void handleSetLocation() {
  bool ok = setLocation(server.arg("country"), server.arg("postcode"));
  server.sendHeader("Location", ok ? "/?msg=ok" : "/?msg=err");
  server.send(303);
}

void startWebInterface() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/info", HTTP_GET, handleInfo);
  server.on("/location", HTTP_POST, handleSetLocation);
  server.begin();
}

void handleWebInterface() { server.handleClient(); }
