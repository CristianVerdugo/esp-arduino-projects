#include "wifi_cc3000.h"
#include <SPI.h>

static Adafruit_CC3000 cc3000(CC3000_CS_PIN, CC3000_IRQ_PIN, CC3000_VBAT_PIN, SPI_CLOCK_DIVIDER);

static bool moduleStarted = false;
static bool ipConfigured = false;
static bool linkUp = false;
static bool pingVerified = false;
static uint8_t pingFailures = 0;
static uint8_t failedAttempts = 0;
static unsigned long lastAttempt = 0;
static unsigned long lastCheck = 0;
static unsigned long lastPing = 0;
static uint32_t gatewayIp = 0;

static void printIp(uint32_t ip)
{
    Serial.print((uint8_t)(ip >> 24));
    Serial.print('.');
    Serial.print((uint8_t)(ip >> 16));
    Serial.print('.');
    Serial.print((uint8_t)(ip >> 8));
    Serial.print('.');
    Serial.println((uint8_t)ip);
}

Adafruit_CC3000 &wifiDevice()
{
    return cc3000;
}

// First call initializes the module; later calls hard-restart it, the only reliable way to recover a stuck CC3000.
static bool startModule()
{
    if (!moduleStarted)
    {
        Serial.println(F("[WIFI] Initializing CC3000..."));
        if (!cc3000.begin())
        {
            Serial.println(F("[WIFI] Init failed."));
            return false;
        }
        moduleStarted = true;

        uint8_t major, minor;
        if (cc3000.getFirmwareVersion(&major, &minor))
        {
            Serial.print(F("[WIFI] Firmware "));
            Serial.print(major);
            Serial.print('.');
            Serial.println(minor);
        }
    }
    else
    {
        Serial.println(F("[WIFI] Restarting CC3000..."));
        cc3000.disconnect();
        cc3000.reboot();
    }

    if (!ipConfigured)
    {
        // Both calls restart the module internally, so the mode is active immediately
#if WIFI_USE_STATIC_IP
        ipConfigured = cc3000.setStaticIPAddress(cc3000.IP2U32(WIFI_STATIC_IP), cc3000.IP2U32(WIFI_STATIC_NETMASK),
                                                 cc3000.IP2U32(WIFI_STATIC_GATEWAY), cc3000.IP2U32(WIFI_STATIC_DNS));
#else
        ipConfigured = cc3000.setDHCP();
#endif
        if (!ipConfigured)
        {
            Serial.println(F("[WIFI] IP config failed."));
            return false;
        }
    }
    return true;
}

static bool attemptConnect()
{
    linkUp = false;
    if (!startModule())
        return false;

    Serial.print(F("[WIFI] Connecting to "));
    Serial.println(F(WIFI_NETWORK));
    cc3000.connectToAP(WIFI_NETWORK, WIFI_PASSWORD, WIFI_SECURITY, 1);

    // The library's connected/DHCP flags survive a module restart, so query the module itself
    unsigned long start = millis();
    while (cc3000.getStatus() != STATUS_CONNECTED)
    {
        if (millis() - start > WIFI_ASSOC_TIMEOUT_MS)
        {
            Serial.println(F("[WIFI] Association failed."));
            return false;
        }
        delay(250);
    }

    uint32_t ip, netmask, dhcpServer, dnsServer;
    start = millis();
    while (!cc3000.getIPAddress(&ip, &netmask, &gatewayIp, &dhcpServer, &dnsServer))
    {
        if (millis() - start > WIFI_DHCP_TIMEOUT_MS)
        {
            Serial.println(F("[WIFI] No IP."));
            return false;
        }
        delay(250);
    }
    // Library workaround: UDP only works after checkDHCP() has run once
    cc3000.checkDHCP();

    Serial.print(F("[WIFI] IP: "));
    printIp(ip);
    linkUp = true;
    failedAttempts = 0;
    pingFailures = 0;
    lastCheck = lastPing = millis();
    return true;
}

static unsigned long retryDelay()
{
    unsigned long d = WIFI_RETRY_MIN_MS;
    for (uint8_t i = 1; i < failedAttempts && d < WIFI_RETRY_MAX_MS; i++)
        d *= 2;
    return d < WIFI_RETRY_MAX_MS ? d : WIFI_RETRY_MAX_MS;
}

// The association flag can stay set while the link is dead, so the gateway is pinged periodically.
static bool linkHealthy(unsigned long now)
{
    if (!cc3000.checkConnected())
    {
        Serial.println(F("[WIFI] Disconnected."));
        return false;
    }
    if (now - lastPing < WIFI_PING_INTERVAL_MS)
        return true;
    lastPing = now;

    if (cc3000.ping(gatewayIp, 1, 500, 32) > 0)
    {
        pingVerified = true;
        pingFailures = 0;
        return true;
    }
    // A gateway that never answers ping must not cause endless restarts
    if (!pingVerified || ++pingFailures < WIFI_PING_MAX_FAILURES)
        return true;

    Serial.println(F("[WIFI] Gateway unreachable."));
    return false;
}

bool wifiConnect()
{
    lastAttempt = millis();
    if (attemptConnect())
        return true;
    failedAttempts = 1;
    return false;
}

bool wifiOnline()
{
    return linkUp && cc3000.checkConnected();
}

bool wifiIsConnected()
{
    return wifiOnline();
}

bool wifiReconnectIfLost()
{
    unsigned long now = millis();

    if (linkUp)
    {
        if (now - lastCheck < WIFI_CHECK_INTERVAL_MS)
            return false;
        lastCheck = now;
        if (linkHealthy(now))
            return false;
        linkUp = false;
        failedAttempts = 0; // reconnect right away
    }
    else if (now - lastAttempt < retryDelay())
    {
        return false;
    }

    lastAttempt = now;
    if (attemptConnect())
    {
        Serial.println(F("[WIFI] Connected."));
        return true;
    }

    if (failedAttempts < 255)
        failedAttempts++;
    Serial.print(F("[WIFI] Retry in "));
    Serial.print(retryDelay() / 1000);
    Serial.println(F(" s"));
    lastAttempt = millis();
    return false;
}

void wifiDisconnect()
{
    linkUp = false;
    cc3000.disconnect();
}
