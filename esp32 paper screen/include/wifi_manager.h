#pragma once

// Connects to the configured Wi-Fi network; returns true when connected.
bool connectWiFi();

// Reconnects if the connection dropped; returns true when connected.
bool ensureWiFi();
