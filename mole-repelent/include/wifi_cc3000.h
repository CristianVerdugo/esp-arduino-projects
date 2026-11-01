#pragma once

#include <Arduino.h>
#include <Adafruit_CC3000.h>

// ---------------------------------------------------------------------------
// Credentials: fill in your network details
// ---------------------------------------------------------------------------
#define WIFI_NETWORK "LosPotitos"
#define WIFI_PASSWORD "macrioliAlla!"
// WLAN_SEC_UNSEC, WLAN_SEC_WEP, WLAN_SEC_WPA or WLAN_SEC_WPA2
#define WIFI_SECURITY WLAN_SEC_WPA2

// The CC3000 never completes DHCP on this router, so a fixed address is used.
// Set WIFI_USE_STATIC_IP to 0 to go back to DHCP.
#define WIFI_USE_STATIC_IP 1
#define WIFI_STATIC_IP 192, 168, 68, 77
#define WIFI_STATIC_NETMASK 255, 255, 252, 0
#define WIFI_STATIC_GATEWAY 192, 168, 68, 1
// The router does not answer CC3000 DNS queries, so a public resolver is used
#define WIFI_STATIC_DNS 8, 8, 8, 8

// ---------------------------------------------------------------------------
// CC3000 shield pins (standard Adafruit / compatible shield wiring)
// SPI uses D11 (MOSI), D12 (MISO), D13 (SCK) in addition to these
// ---------------------------------------------------------------------------
#define CC3000_IRQ_PIN 3  // Must be an interrupt pin (D2 or D3)
#define CC3000_VBAT_PIN 5 // Any free pin
#define CC3000_CS_PIN 10  // Any free pin

#define WIFI_ASSOC_TIMEOUT_MS 15000UL
#define WIFI_DHCP_TIMEOUT_MS 20000UL
#define WIFI_CHECK_INTERVAL_MS 5000UL
// Failed attempts back off exponentially from MIN to MAX
#define WIFI_RETRY_MIN_MS 10000UL
#define WIFI_RETRY_MAX_MS 300000UL
#define WIFI_PING_INTERVAL_MS 60000UL
#define WIFI_PING_MAX_FAILURES 3

// Initializes the CC3000 and connects to WIFI_NETWORK. Returns true once an IP is obtained.
bool wifiConnect();

// Call from loop(). Returns true only when a lost connection was just re-established,
// in which case listening sockets must be reopened.
bool wifiReconnectIfLost();

// True while the module is associated with the access point and has an IP.
bool wifiIsConnected();

// Cheap association check (no SPI traffic), safe to call every loop.
bool wifiOnline();

// Underlying driver, for opening TCP clients (connectTCP / getHostByName).
Adafruit_CC3000 &wifiDevice();

// Disconnects and powers down the module.
void wifiDisconnect();
