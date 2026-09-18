# Sources & references

Every external source consulted for design decisions in this project, and what
was taken from each. Read this before "improving" anything — most sharp edges
here were found the hard way.

## ESP8266 Arduino core (arduino-esp8266.readthedocs.io)

- **Boot-mode / strapping pins** (`boards.rst`, `reference.rst`): GPIO0/2/15
  decide boot mode; GPIO6–11 are the flash bus; GPIO1/3 are UART0. Source of
  the pin map in `src/config.h`.
- **`ESP.rtcUserMemoryRead/Write`**: 512 bytes of RTC RAM survive resets
  (button, `ESP.restart()`, deep sleep) but NOT power loss. Basis of
  triple-reset recovery (`src/device/reset_recovery.cpp`). Words 64+ used to
  stay clear of the SDK-reserved low region.
- **`Updater` class (`Update.begin/writeStream/end`)**: used verbatim for both
  manual upload and GitHub OTA. `ESP.getFreeSketchSpace()` sizes the OTA
  partition (`maxSketch` computation copied from the core's own
  `ESP8266HTTPUpdateServer` example).
- **BearSSL / `WiFiClientSecure`**: `setInsecure()` + `setTimeout()` for
  GitHub HTTPS on a 80 MHz chip. Certificate validation is skipped (see
  Security notes in README); the flashed image is still verified by
  `Update.end(true)`.
- **LittleFS**: SPIFFS is deprecated in core 3.x — all persistence uses
  LittleFS (`board_build.filesystem = littlefs`).
- **No exceptions**: the Xtensa libstdc++ here is built with
  `-fno-exceptions`. `LogStore::load` validates digits by hand instead of
  `try/catch` + `stoul` (a real CI failure taught us this).

## tzapu / WiFiManager (GitHub)

Captive-portal provisioning: `autoConnect()` with timeout for first boot,
`startConfigPortal()` for on-demand (switch-hold + web button). Timeouts keep
every portal call bounded so the loop never blocks forever.

## arduino-libraries / NTPClient + PaulStoffregen / Time

`forceUpdate()` with retries at boot (timestamps valid from minute one),
`update()` on a 5-minute tick, `setTime()` + `TimeLib` getters for formatting.
Offset 19800 s = Asia/Kolkata.

## khoih-prog / ESP_DoubleResetDetector (concept)

The "press reset N times to recover" pattern. We implement triple-reset
directly on RTC memory instead of taking the library (fewer dependencies,
threshold of 3 instead of 2 — a double press happens by accident; triple
doesn't).

## Jack Ganssle — "A Guide to Debouncing"

Mechanical contacts bounce for ~10–20 ms; 50 ms settle time with edge
detection is the industry default. `HA_DEBOUNCE_MS = 50`, hold-repeat
suppressed (one press = one toggle, proven by the bounce-storm unit test).

## GitHub REST API — "Get the latest release"

`GET /repos/{owner}/{repo}/releases/latest` returns `tag_name` + `assets[]`
with `browser_download_url`. The device needs only two fields, so parsing is a
20-line scanner (`src/logic/ghota.cpp`) instead of a JSON library — fully
host-tested, zero flash cost.

## OWASP Password Storage Cheat Sheet

Salted hashing, never plaintext; per-device salt (chip ID); force-change of
any default on first boot. SHA-256 (not bcrypt — an ESP8266 can't afford it;
threat model is LAN, documented in README §15).

## semver.org

Version comparison semantics: numeric per-component, release beats
pre-release. `compareVersions()` follows it loosely (tags may carry a `v`
prefix and two-part numbers like `1.01`).

## PlatformIO docs — Unit Testing with Unity

`native` env + one suite per module + implementation compiled into the test
binary (PIO links test programs without `src/` objects — hence the
`#include "logic/*.cpp"` pattern in `test/`).

## Arduino sketch rules (arduino.cc + arduino-builder)

The IDE auto-generates function prototypes and inserts them near the top of
the `.ino` — above any types defined later in the file. Consequence, enforced
by `tools/check_ino.sh`: **free functions in device code must not use custom
types in signatures** (roles are `int`, contexts are forward-declared in the
mirror preamble). `struct RtcSlot` and `struct AppContext` forward
declarations in `tools/mirror_to_ino.py` exist for exactly this reason.

## OWASP Authentication + Session Management Cheat Sheets

- **Session tokens over Basic auth**: per-request passwords replay the secret
  and can't be revoked; opaque, random, expirable server-side sessions can.
  128-bit tokens from `ESP.random()` (RF-noise seeded), 30-min sliding
  expiry, cap + sweep (`src/logic/session.h`).
- **Brute-force defense**: count failures per identifier, sliding window,
  temporary lockout, log with IP — and never reveal whether the username or
  the password was wrong (`src/logic/guard.h`, flat "Bad login").
- **CSRF**: SameSite cookies + Origin/Referer validation on state-changing
  requests (OWASP CSRF Prevention Cheat Sheet, "custom headers / origin
  check" branch — no per-form tokens needed for this size).

## MDN Web Docs — Set-Cookie, SameSite, CSP

`HttpOnly; SameSite=Strict; Max-Age=1800; Path=/` semantics; `X-Frame-Options:
DENY`, `X-Content-Type-Options: nosniff`, `Content-Security-Policy`
(`frame-ancestors 'none'`, `object-src 'none'`). `Secure` is omitted
deliberately — no HTTPS server (see README §15 for the reasoning).

## ESP8266WebServer examples (FSBrowser, HTTPUpdateServer)

Authenticated-upload pattern: `server.upload()` chunks → `Update.write`
during `UPLOAD_FILE_WRITE` → `Update.end(true)` at `UPLOAD_FILE_END`, auth
decided at `UPLOAD_FILE_START` from the `Authorization` header (collected via
`collectHeaders`). Legacy `POST /relay/<i>` is parsed manually in the
not-found handler because core 3.1.x has no `UriBraces`.
