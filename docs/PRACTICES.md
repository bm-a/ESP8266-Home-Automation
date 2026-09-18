# Good practices used here (and a few worth knowing)

## Practices applied in this codebase

1. **No `delay()` in the hot loop.** Everything periodic is a rollover-safe
   `Every` timer (`src/logic/scheduler.h`). A blocked loop misses switches,
   starves the web server, and trips the watchdog.
2. **Separate logic from hardware.** `src/logic/` is plain C++ with zero
   Arduino dependencies, so 46 assertions run on the dev machine in seconds.
   Rule of thumb: if it doesn't touch a pin or a radio, it belongs in logic.
3. **Debounce + edge-detect every mechanical input.** Level-polling a switch
   is a flash-wear and log-spam bug (v3 did this). See Ganssle reference in
   SOURCES.md.
4. **Throttle flash writes.** Logs flush every 25 events or 60 s; relay state
   is one byte. Flash has ~100k erase cycles — batch or die.
5. **Cap everything.** Log rotation is both age- AND size-bounded; the parser
   rejects overlong frames; upload size is checked before `Update.begin`.
6. **Fail closed.** The setup gate denies all mutating routes until the admin
   password exists; unknown auth roles get 401; OTA upload defaults to
   not-allowed until admin is proven at `UPLOAD_FILE_START`.
7. **Salted hashes, no plaintext.** Passwords never touch disk or logs in the
   clear — not even in serialized form (there's a unit test asserting it).
8. **Distinguish intentional reboots from button presses.** RTC disarm before
   every `ESP.restart()` — otherwise self-reboots would count toward the
   triple-reset detector. Any reset-counter design needs this.
9. **Test the boring stuff.** Rollover (`0xFFFFFF00`), malformed input,
   version compare, JSON garbage, million-byte hashes — the bugs live in the
   boring stuff.
10. **Soak simulates time, not just cases.** 30 virtual days with switch
    storms, WiFi drops and a millis() wrap in 4 seconds (`tools/soak_sim.cpp`).
11. **One source of truth, enforced.** The Arduino sketch is generated
    (`mirror_to_ino.py`) and CI fails if it's stale; `check_ino.sh` proves the
    generated file actually compiles.
12. **`.gitignore` the build cache.** `.pio/` is hundreds of MB and
    machine-specific — it once got committed here by accident.
13. **Sessions, not per-request secrets.** Basic auth replays the password on
    every call and can't be revoked short of changing it. Opaque tokens with
    expiry + explicit logout are barely more code.
14. **Rate-limit authentication.** Any login endpoint without a lockout is a
    password-guessing oracle. Count per IP, sliding window, temporary lock,
    log with source.
15. **Allowlist outbound fetch targets.** A device that downloads URLs must
    never fetch attacker-chosen hosts (SSRF) — especially one that then
    flashes what it fetched.
16. **Send secure headers by default.** One wrapper (`sendSecure`) around
    every response beats remembering per handler.

## Practices worth knowing (not yet applied)

- **Move constant strings to flash (`PROGMEM`/`FPSTR`).** The web UI's HTML
  lives in RAM (we're at 48%). Fine today; if RAM ever passes 70%, move `kCss`
  and page templates to PROGMEM first.
- **Feed the watchdog deliberately.** The core's software watchdog reboots a
  wedged loop. Our loops are short, but any future blocking call (>3 s)
  needs `yield()`/`delay()` inside it or `ESP.wdtFeed()`.
- **Static analysis in CI.** `cppcheck --enable=all src/logic/` takes seconds
  and catches the `sprintf`-without-`<cstdio>` class of bug before GCC does.
- **Fuzz the parsers.** Feed random bytes to `LogStore::load` and
  `ghota::parseLatest` in a loop with sanitizers (`-fsanitize=address,undefined`).
- **Hardware-in-the-loop tests.** A second ESP8266 (or a USB relay + pytest,
  as in the sister BMS-tester project) running the real routes nightly.
- **Pin `platformio.ini` versions and commit the lockfile.** We pin
  (`WiFiManager@^2.0.17` etc.); `^` still allows minor drift — exact pins
  (`=`) are stricter for reproducible factory builds.
