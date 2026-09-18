// WiFi connection via WiFiManager captive portal (device only).
#pragma once
#include <string>

// Blocking connect with portal fallback (call at boot).
// Returns true if STA connected. Portal SSID gets a chip-id suffix.
bool wifiPortalBoot(const char* baseApName = "ESP-Setup",
                    int portalTimeoutS = 300);

// Non-blocking check; call every few seconds to update status.
void wifiPortalPoll();

// Open the config portal on demand (long-press / web button).
void wifiPortalOpen(const char* baseApName = "ESP-Setup",
                    int portalTimeoutS = 300);

bool wifiIsUp();
std::string wifiIp();
std::string wifiApName(const char* base);
