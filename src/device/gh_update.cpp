#include "gh_update.h"
#include <ESP8266HTTPClient.h>
#include <Updater.h>
#include <WiFiClientSecure.h>
#include "../logic/ghota.h"

static const char* kApiHost = "api.github.com";
static const int kApiPort = 443;

static void applySecure(WiFiClientSecure& c) {
  c.setInsecure();
  c.setTimeout(15000);
}

bool ghCheckLatest(std::string& tagOut, std::string& binUrlOut,
                   std::string* bodyOut) {
  tagOut.clear();
  binUrlOut.clear();
  WiFiClientSecure client;
  applySecure(client);
  HTTPClient http;
  std::string path = std::string("/repos/") + HA_GH_OWNER + "/" + HA_GH_REPO +
                     "/releases/latest";
  if (!http.begin(client, kApiHost, kApiPort, path.c_str(), true)) return false;
  http.setUserAgent("esp-home-ota");
  http.setTimeout(15000);
  http.addHeader("Accept", "application/vnd.github+json");
  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    http.end();
    return false;
  }
  String body = http.getString();
  http.end();
  std::string raw(body.c_str(), body.length());
  if (bodyOut) *bodyOut = raw;
  return ha::ghota::parseLatest(raw, tagOut, binUrlOut);
}

bool ghDownloadAndFlash(const std::string& binUrl,
                        void (*logFn)(const std::string&)) {
  if (binUrl.empty()) return false;
  WiFiClientSecure client;
  applySecure(client);
  HTTPClient http;
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
  http.setUserAgent("esp-home-ota");
  http.setTimeout(20000);
  if (!http.begin(client, binUrl.c_str())) return false;
  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    http.end();
    return false;
  }
  int total = http.getSize();
  if (total <= 0) {
    http.end();
    return false;
  }
  uint32_t maxSketch = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
  if (!Update.begin(maxSketch)) {
    http.end();
    return false;
  }
  size_t written = Update.writeStream(http.getStream());
  bool ok = (written == (size_t)total) && Update.end(true);
  if (logFn) {
    char msg[96];
    snprintf(msg, sizeof(msg), "GitHub OTA %s (%u bytes)",
             ok ? "applied" : "FAILED", (unsigned)written);
    logFn(msg);
  }
  http.end();
  return ok && !Update.hasError();
}
