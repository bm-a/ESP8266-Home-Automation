#include "time_sync.h"
#include <ESP8266WiFi.h>
#include <NTPClient.h>
#include <TimeLib.h>
#include <WiFiUdp.h>
#include "../config.h"

static WiFiUDP s_udp;
static NTPClient s_ntp(s_udp, HA_NTP_SERVER, HA_NTP_OFFSET_S, HA_NTP_INTERVAL_MS);
static bool s_valid = false;

void timeSyncBegin() {
  s_ntp.begin();
}

bool timeSyncForceUpdate(int retries) {
  for (int i = 0; i < retries; i++) {
    if (s_ntp.forceUpdate()) {
      setTime(s_ntp.getEpochTime());
      s_valid = true;
      return true;
    }
    delay(500);
  }
  return false;
}

void timeSyncLoop() {
  if (WiFi.status() != WL_CONNECTED) return;
  if (s_ntp.update()) {
    setTime(s_ntp.getEpochTime());
    s_valid = true;
  }
}

uint32_t timeEpoch() { return s_valid ? (uint32_t)s_ntp.getEpochTime() : 0; }
bool timeIsValid() { return s_valid; }

std::string timeFormat(uint32_t epoch) {
  char buf[24];
  if (epoch == 0) {
    snprintf(buf, sizeof(buf), "[no-time]");
    return std::string(buf);
  }
  // Break down without touching the global TimeLib clock.
  uint32_t days = epoch / 86400UL;
  uint32_t secs = epoch % 86400UL;
  // Civil-from-days (Howard Hinnant algorithm), days since 1970-01-01.
  int64_t z = (int64_t)days + 719468;
  int64_t era = (z >= 0 ? z : z - 146096) / 146097;
  uint64_t doe = (uint64_t)(z - era * 146097);
  uint64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  int64_t y = (int64_t)yoe + era * 400;
  uint64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  uint64_t mp = (5 * doy + 2) / 153;
  uint64_t d = doy - (153 * mp + 2) / 5 + 1;
  uint64_t m = mp + (mp < 10 ? 3 : -9);
  y += (m <= 2);
  snprintf(buf, sizeof(buf), "[%04d-%02d-%02d %02d:%02d:%02d]", (int)y, (int)m,
           (int)d, (int)(secs / 3600), (int)((secs % 3600) / 60),
           (int)(secs % 60));
  return std::string(buf);
}
