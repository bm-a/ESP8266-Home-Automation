#include "reset_recovery.h"
#include <ESP8266WiFi.h>

static const uint32_t kMagic = 0x48415243;  // "HARC"
static const uint32_t kRtcOffset = 64;      // words; above SDK-reserved area

struct RtcSlot {
  uint32_t magic;
  uint32_t count;
  uint32_t spare;
};

static bool rtcRead(RtcSlot& s) {
  if (!ESP.rtcUserMemoryRead(kRtcOffset, (uint32_t*)&s, sizeof(s) / 4))
    return false;
  return s.magic == kMagic;
}

static void rtcWrite(const RtcSlot& s) {
  ESP.rtcUserMemoryWrite(kRtcOffset, (uint32_t*)&s, sizeof(s) / 4);
}

int resetRecoveryCount() {
  RtcSlot s{0, 0, 0};
  if (!rtcRead(s)) return 0;
  return (int)s.count;
}

void resetRecoverySave(int count) {
  RtcSlot s{kMagic, (uint32_t)(count < 0 ? 0 : count), 0};
  rtcWrite(s);
}

void resetRecoveryClear() {
  RtcSlot s{0, 0, 0};  // bad magic == "no history"
  rtcWrite(s);
}

void resetRecoveryDisarm() { resetRecoveryClear(); }
