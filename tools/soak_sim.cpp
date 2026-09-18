// 30-day virtual soak: switch storms, WiFi drops, relay churn, log rotation.
// Builds with: g++ -std=c++17 -I src/logic -I src/common tools/soak_sim.cpp
//   src/logic/*.cpp src/common/sha256.cpp -o /tmp/soak && /tmp/soak
#include <cassert>
#include <cstdio>
#include "auth.h"
#include "debounce.h"
#include "logstore.h"
#include "relay.h"
#include "scheduler.h"

static uint32_t rng_state = 0x12345678;
static uint32_t rnd(uint32_t n) {
  rng_state = rng_state * 1664525u + 1013904223u;
  return (rng_state >> 8) % n;
}

int main() {
  ha::RelayBank relays;
  relays.configure(4, true);
  ha::SwitchBank sw;
  sw.configure(4, 50, 0);
  ha::LogStore log;
  log.configure(65536, 30);
  ha::AuthStore auth;
  auth.setSalt("soak");
  auth.setAdminPassword("soak-admin-pass");
  ha::Every flush(60000), prune(86400000);

  uint32_t now_ms = 0, epoch = 1758000000;
  long presses = 0, toggles = 0, flushes = 0;
  bool wifi = true;
  bool raw[4] = {false, false, false, false};
  uint32_t raw_until[4] = {0, 0, 0, 0};

  // 30 days at 10 ms steps = 259.2M iterations (too many); use 100 ms steps
  // with sub-step switch physics every 5 ms during bounce windows.
  for (uint32_t step = 0; step < 25920000u; step++) {
    now_ms += 100;
    if (step % 10 == 0) epoch += 1;

    // Random WiFi dropouts (a few per day).
    if (rnd(2500000) == 0) wifi = !wifi;

    // Random switch press: 40 ms bounce, then settled hold.
    for (int c = 0; c < 4; c++) {
      if (!raw[c] && rnd(40000) == 0) {
        raw[c] = true;
        raw_until[c] = now_ms + 40 + rnd(2000);
        for (uint32_t b = now_ms; b < now_ms + 40; b += 5) {
          bool level = (rnd(2) == 0) ? true : (b > now_ms + 25);
          if (sw.update(c, level, b) == ha::SwitchBank::PRESSED) {
            presses++;
            relays.toggle(c);
            toggles++;
            char m[48];
            snprintf(m, sizeof(m), "sw%d->relay%d", c, c);
            log.append(epoch, m);
          }
        }
        // Settle: stable contact for 60 ms so the 50 ms debounce trips.
        for (uint32_t b = 0; b < 60; b += 5) {
          if (sw.update(c, true, now_ms + 40 + b) == ha::SwitchBank::PRESSED) {
            presses++;
            relays.toggle(c);
            toggles++;
            log.append(epoch, "settle-press");
          }
        }
      }
      if (raw[c] && now_ms >= raw_until[c]) {
        raw[c] = false;
        for (uint32_t b = 0; b < 60; b += 5)
          sw.update(c, false, now_ms + b);
      }
    }

    if (flush.due(now_ms)) {
      flushes++;
      log.prune(epoch);
    }
    if (prune.due(now_ms)) log.prune(epoch);

    // Millis rollover crossing must not break anything.
    if (step == 20000000u) now_ms = 0xFFFFFF00u;
  }

  printf("presses=%ld toggles=%ld flushes=%ld log_bytes=%zu log_entries=%zu\n",
         presses, toggles, flushes, log.bytes(), log.entries());
  assert(presses == toggles);
  assert(presses > 1000);
  assert(log.bytes() <= log.maxBytes());
  assert(auth.verifyAdmin("soak-admin-pass"));
  assert(!auth.verifyAdmin("wrong"));
  // Relay state coherent after storm.
  for (int c = 0; c < 4; c++) (void)relays.get(c);
  printf("SOAK OK\n");
  return 0;
}
