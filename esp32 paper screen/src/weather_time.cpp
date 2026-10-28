#include "weather_time.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFiClientSecure.h>
#include <time.h>

static const char *DEFAULT_COUNTRY_CODE = "de";
static const char *DEFAULT_POST_CODE = "91325"; // Adelsdorf

static String countryCode = DEFAULT_COUNTRY_CODE;
static String postCode = DEFAULT_POST_CODE;

static const unsigned long WEATHER_INTERVAL_MS = 10UL * 60UL * 1000UL;
static const unsigned long TIME_INTERVAL_MS = 10UL * 1000UL;

static WeatherData weather;
static float latitude = 0;
static float longitude = 0;
static bool haveLocation = false;
static unsigned long lastWeather = 0;
static unsigned long lastTimePrint = 0;

// HTTPS GET into a JSON document; certificate is not verified.
static bool httpGetJson(const String &url, JsonDocument &doc) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  if (!http.begin(client, url)) return false;

  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    Serial.printf("HTTP error %d for %s\n", code, url.c_str());
    http.end();
    return false;
  }
  String body = http.getString(); // decodes chunked encoding
  http.end();
  DeserializationError err = deserializeJson(doc, body);
  if (err) {
    Serial.printf("JSON error: %s\nBody: %s\n", err.c_str(), body.c_str());
    return false;
  }
  return true;
}

static bool validLocationInput(const String &country, const String &post) {
  if (country.length() != 2 || post.length() == 0 || post.length() > 12) return false;
  for (char c : country) if (!isalpha((unsigned char)c)) return false;
  for (char c : post) if (!isalnum((unsigned char)c) && c != ' ' && c != '-') return false;
  return true;
}

static bool lookupPostCode(const String &country, const String &post) {
  JsonDocument doc;
  String encoded = post;
  encoded.replace(" ", "%20");
  String url = String("https://api.zippopotam.us/") + country + "/" + encoded;
  if (!httpGetJson(url, doc)) return false;

  JsonObject place = doc["places"][0];
  if (place.isNull()) return false;
  latitude = atof(place["latitude"] | "0");
  longitude = atof(place["longitude"] | "0");
  weather.placeName = (const char *)(place["place name"] | "?");
  Serial.printf("Location: %s (%.4f, %.4f)\n", weather.placeName.c_str(), latitude, longitude);
  return true;
}

static bool fetchWeather() {
  JsonDocument doc;
  String url = String("https://api.open-meteo.com/v1/forecast?latitude=") + latitude +
               "&longitude=" + longitude +
               "&current=temperature_2m,relative_humidity_2m,wind_speed_10m,weather_code"
               "&timezone=auto";
  if (!httpGetJson(url, doc)) return false;

  JsonObject cur = doc["current"];
  weather.temperature = cur["temperature_2m"].as<float>();
  weather.humidity = cur["relative_humidity_2m"].as<int>();
  weather.windSpeed = cur["wind_speed_10m"].as<float>();
  weather.weatherCode = cur["weather_code"] | 0;
  weather.valid = true;
  Serial.printf("Weather: %.1f C, %s, humidity %d%%, wind %.1f km/h\n", weather.temperature,
                weatherText(weather.weatherCode), weather.humidity, weather.windSpeed);

  // Sync clock using the local UTC offset reported for this location.
  configTime(doc["utc_offset_seconds"] | 0, 0, "pool.ntp.org", "time.nist.gov");
  return true;
}

const WeatherData &getWeather() { return weather; }

const char *getPostCode() { return postCode.c_str(); }

const char *getCountryCode() { return countryCode.c_str(); }

void loadSavedLocation() {
  Preferences prefs;
  prefs.begin("weather", true);
  String c = prefs.getString("country", DEFAULT_COUNTRY_CODE);
  String p = prefs.getString("post", DEFAULT_POST_CODE);
  prefs.end();
  if (validLocationInput(c, p)) {
    countryCode = c;
    postCode = p;
  }
}

bool setLocation(const String &country, const String &post) {
  String c = country, p = post;
  c.trim();
  p.trim();
  c.toLowerCase();
  if (!validLocationInput(c, p)) return false;

  // Remember the old coordinates so a failed lookup leaves the current location intact.
  float oldLat = latitude, oldLon = longitude;
  String oldName = weather.placeName;
  if (!lookupPostCode(c, p)) {
    latitude = oldLat;
    longitude = oldLon;
    weather.placeName = oldName;
    return false;
  }

  countryCode = c;
  postCode = p;
  Preferences prefs;
  prefs.begin("weather", false);
  prefs.putString("country", c);
  prefs.putString("post", p);
  prefs.end();

  haveLocation = true;
  weather.valid = false;
  fetchWeather();
  lastWeather = millis();
  return true;
}

const char *weatherText(int code) {
  if (code == 0) return "Clear sky";
  if (code <= 3) return "Partly cloudy";
  if (code <= 48) return "Fog";
  if (code <= 57) return "Drizzle";
  if (code <= 67) return "Rain";
  if (code <= 77) return "Snow";
  if (code <= 82) return "Rain showers";
  if (code <= 86) return "Snow showers";
  return "Thunderstorm";
}

bool getTimeString(char *buf, size_t len, uint32_t timeoutMs) {
  struct tm t;
  if (!getLocalTime(&t, timeoutMs)) return false;
  strftime(buf, len, "%Y-%m-%d %H:%M:%S", &t);
  return true;
}

void updateWeatherAndTime() {
  if (!haveLocation) haveLocation = lookupPostCode(countryCode, postCode);

  if (haveLocation && (!weather.valid || millis() - lastWeather >= WEATHER_INTERVAL_MS)) {
    fetchWeather();
    lastWeather = millis();
  }

  if (millis() - lastTimePrint >= TIME_INTERVAL_MS) {
    char buf[24];
    if (getTimeString(buf, sizeof(buf), 2000)) {
      Serial.printf("Time: %s\n", buf);
    } else {
      Serial.println("Time not synced yet");
    }
    lastTimePrint = millis();
  }
}
