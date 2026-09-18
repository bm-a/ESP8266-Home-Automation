#include "wifi_portal.h"
#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include "../config.h"

static bool s_up = false;

std::string wifiApName(const char* base) {
  uint32_t id = ESP.getChipId();
  char name[32];
  snprintf(name, sizeof(name), "%s-%04X", base, (unsigned)(id & 0xFFFF));
  return std::string(name);
}

bool wifiPortalBoot(const char* baseApName, int portalTimeoutS) {
  WiFi.mode(WIFI_STA);
  WiFiManager wm;
  wm.setTitle("ESP Home Setup");
  wm.setConfigPortalTimeout(portalTimeoutS);
  wm.setConnectTimeout(20);
  std::string ap = wifiApName(baseApName);
  s_up = wm.autoConnect(ap.c_str());
  return s_up;
}

void wifiPortalPoll() { s_up = (WiFi.status() == WL_CONNECTED); }

void wifiPortalOpen(const char* baseApName, int portalTimeoutS) {
  WiFiManager wm;
  wm.setTitle("ESP Home Setup");
  wm.setConfigPortalTimeout(portalTimeoutS);
  std::string ap = wifiApName(baseApName);
  wm.startConfigPortal(ap.c_str());
  wifiPortalPoll();
}

bool wifiIsUp() { return s_up; }

std::string wifiIp() {
  if (!s_up) return std::string("0.0.0.0");
  return std::string(WiFi.localIP().toString().c_str());
}
