#pragma once

#include <Arduino.h>

// Syncs with an NTP server when never synced or the last sync is stale. Call from loop() once WiFi is up.
void ntpUpdate();

// True once at least one NTP sync succeeded.
bool ntpHasTime();

// Writes Berlin local time ("YYYY-MM-DD HH:MM:SS CET|CEST") into buf (>= 28 bytes), or "not synced".
void berlinTimeString(char *buf, size_t len);

// Gets the current hour in Berlin local time (0-23). Returns false if time not synced yet.
bool getHourBerlin(uint8_t &hour);
