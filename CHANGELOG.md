# Changelog

## v4.0.0 (2026-09-19) — full rewrite
- PlatformIO project: modular `src/` (logic / device / common) + Unity tests
  (34 assertions) + 30-day soak sim + CI.
- Correct ESP8266 pin map: relays GPIO 5/4/12/13, switches GPIO 14/16/0/2
  (was: TX pin + raw GPIO numbers, serial conflict).
- Captive-portal WiFi (WiFiManager, `ESP-Setup-XXXX`) with offline standalone
  mode instead of boot-hang on empty SSID; on-demand portal via 8 s switch-1
  hold or web button.
- Working routes: JSON `POST /api/relay`, `GET /api/state`; legacy
  `POST /relay/<i>` shape manually routed (core 3.1.x has no UriBraces).
- Real OTA: authenticated HTTP upload (`/update`) + ArduinoOTA with password.
- Event log that works: edge-detected debounced switches, timestamp valid from
  boot NTP force-sync, 30-day + 64 KB rotation, throttled flash writes.
- Security: no default passwords — forced admin setup on first boot; salted
  SHA-256 storage; user role gated; setup gate on all mutating routes.
- Relay states persist across reboots; `RELAY_ACTIVE_LOW` flag; mDNS
  (`esp-home.local`); 15-min keepalive probe for idle-dropping hotspots.
- Arduino IDE path: generated single-file sketch in `arduino/` (same code,
  compile-verified; no custom types in signatures so IDE prototype hoisting
  can't break it).
- Removed: hostel-network iframe, fake TODO handlers.

## v3 and earlier — see `legacy/`
Original single-file sketch (`v3_updated_with_ota_and_fixes.ino`, kept as
`legacy/v3_updated_with_ota_and_fixes.ino`). Known defects fixed by v4:
unmatched `/relay/<index>` route, TODO WiFi/hotspot handlers, never-started
hotspot, blocking connect on empty SSID, broken OTA upload (no Update
machinery), `timestamp[20]` overflow, no-op log pruning, SPIFFS spam from
un-debounced switches, relay on TX pin, hardcoded admin/admin + user/user.
