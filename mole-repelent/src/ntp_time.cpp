#include "ntp_time.h"
#include "wifi_cc3000.h"

// Boot-time assumption: assume it's 8 AM when board starts
// This allows motor to work offline, but will be corrected if WiFi syncs
static const uint8_t BOOT_HOUR_ASSUMPTION = 8; // 8 AM default

static unsigned long bootMillis = 0;
static uint32_t bootUnixTime = 0;  // Will be synced when WiFi available
static bool timeSynced = false;

// Attempt to sync time from NTP (only when WiFi is online)
static bool queryNtp(uint32_t &unixTime)
{
    // Only attempt if WiFi is online
    if (!wifiOnline())
        return false;

    Serial.println(F("[NTP] Querying time..."));
    
    // Try to get IP of NTP server via DNS
    uint32_t ip = 0;
    wifiDevice().getHostByName("pool.ntp.org", &ip);
    
    if (ip == 0)
    {
        // DNS failed, use fallback Google NTP
        ip = wifiDevice().IP2U32(216, 239, 35, 0);
    }

    // Create UDP connection
    Adafruit_CC3000_Client udp = wifiDevice().connectUDP(ip, 123);
    if (!udp.connected())
    {
        return false;
    }

    // Send NTP request
    uint8_t packet[48] = {0x1B};
    udp.write(packet, sizeof(packet));

    // Wait for reply (1 second timeout)
    unsigned long start = millis();
    while (!udp.available() && millis() - start < 1000)
    {
        delay(10);
    }

    // Read response
    bool ok = udp.available() && udp.read(packet, sizeof(packet)) >= 48;
    udp.close();
    
    if (!ok)
    {
        return false;
    }

    // Extract timestamp from bytes 40-43 (big-endian)
    uint32_t secs = ((uint32_t)packet[40] << 24) | ((uint32_t)packet[41] << 16) |
                    ((uint32_t)packet[42] << 8) | packet[43];
    
    // Convert from NTP epoch (1900) to Unix epoch (1970)
    const uint32_t NTP_TO_UNIX = 2208988800UL;
    if (secs < NTP_TO_UNIX)
        return false;
    
    unixTime = secs - NTP_TO_UNIX;
    return true;
}

void ntpUpdate()
{
    // Ensure boot time is initialized
    if (bootMillis == 0)
    {
        bootMillis = millis();
        bootUnixTime = 0;
    }
    
    // Try to sync time from NTP if not already synced
    if (!timeSynced)
    {
        uint32_t unixTime = 0;
        if (queryNtp(unixTime))
        {
            // Calculate what bootUnixTime should have been
            unsigned long elapsedSeconds = (millis() - bootMillis) / 1000;
            bootUnixTime = unixTime - elapsedSeconds;
            timeSynced = true;
            Serial.println(F("[NTP] Time synced!"));
        }
    }
}

bool ntpHasTime()
{
    return true;  // Always have time estimate
}

void berlinTimeString(char *buf, size_t len)
{
    snprintf_P(buf, len, PSTR("--:--:--"));
}

bool getHourBerlin(uint8_t &hour)
{
    // Ensure bootMillis is initialized
    if (bootMillis == 0)
    {
        bootMillis = millis();
        bootUnixTime = 0;
    }
    
    unsigned long now_s;
    
    if (timeSynced && bootUnixTime > 0)
    {
        // Use synced Unix time
        now_s = bootUnixTime + (millis() - bootMillis) / 1000;
    }
    else
    {
        // Use estimated time (assume boot at 8 AM)
        unsigned long elapsedSeconds = (millis() - bootMillis) / 1000;
        unsigned long elapsedHours = elapsedSeconds / 3600;
        hour = (BOOT_HOUR_ASSUMPTION + elapsedHours) % 24;
        return true;
    }
    
    // Calculate hour from Unix timestamp
    // Unix day starts at 00:00 UTC
    // For Berlin: UTC+1 (winter) or UTC+2 (summer), approximate with +1
    uint32_t daySeconds = now_s % 86400;
    hour = (daySeconds / 3600 + 1) % 24;
    
    return true;
}
