# ESP8266 Home Automation v4

4-channel relay control with physical switches, captive-portal WiFi, authenticated
web UI + JSON API, event logging, NTP time, and dual OTA — on a generic ESP8266
dev module (ESP-12E / NodeMCU / Wemos D1 mini).

> v4 is a from-scratch rewrite. The original v3 sketch is preserved in `legacy/`;
> see `CHANGELOG.md` for what was broken and fixed.

## Wiring

| Function | GPIO | Board label | Notes |
|---|---|---|---|
| Relay 1–4 IN | 5, 4, 12, 13 | D1, D2, D6, D7 | Active-LOW modules assumed (`RELAY_ACTIVE_LOW`, flip if yours differ) |
| Switch 1–4 | 14, 16, 0, 2 | D5, D0, FLASH, D4 | Buttons to GND |
| Relay VCC/JD-VCC | 5 V supply | — | Keep relay coil power separate from ESP 3V3 if 4 relays chatter |
| ESP power | USB 5 V | — | Never mains without an isolated supply |

- **GPIO16 (switch 2)** has no internal pull-up: add a **10 kΩ resistor to 3V3**.
- Switches 1/3/4 use internal pull-ups; GPIO0/2 wiring mirrors the FLASH button (safe).
- Relay board inputs to the GPIOs above; relay VCC to 5 V; common GND.
- Onboard LED (GPIO2) is switch 4's pin — read as input only, never driven.

## First boot

1. Flash (PlatformIO `pio run -e esp12e -t upload`, or Arduino IDE sketch in `arduino/`).
2. Board opens AP **`ESP-Setup-XXXX`** → join it → captive page asks for home WiFi.
3. If WiFi never connects, the board still runs standalone (relays + switches work; portal retries in background).
4. Browse to the IP (or `esp-home.local`) → **set the admin password** (≥ 8 chars). Nothing else works until you do — there are no default logins.
5. Optional: Settings → enable the limited `user` account (relays only), change OTA password.

Hold **switch 1 for 8 s** any time to reopen the WiFi portal (5-min timeout).

## Web UI / API

- `/` dashboard (relay states, live poll), `/logs` (admin), `/settings` (admin), `/update` (admin firmware upload), `/setup` (first boot).
- `GET /api/state` → `{time, ip, rssi, ntp, uptime, relays[]}`.
- `POST /api/relay` with `ch=` + `on=1/0` (omit `on` to toggle); legacy `POST /relay/<i>` toggles.
- Auth: HTTP Basic, roles `admin` (everything) / `user` (relays + state only).

## Build & test

```sh
pio test -e native     # 34 Unity assertions (relay, debounce, log, auth, scheduler, SHA-256)
sh run_tests.sh        # above + 30-day soak sim (switch storms, WiFi drops, millis wrap)
pio run -e esp12e      # firmware
python3 tools/mirror_to_ino.py   # regenerate Arduino sketch after editing src/
```

## Repo layout

```
src/logic/    hardware-independent core (tested on host)
src/device/   ESP8266 drivers: portal, web UI, OTA, NTP, storage
src/common/   shared SHA-256
src/main.cpp  wiring + loop
test/         Unity suites   tools/soak_sim.cpp   tools/mirror_to_ino.py
arduino/      generated single-file sketch (Arduino IDE)
legacy/       original v3 sketch, untouched
```

## Security notes

Passwords are stored salted (chip-id salt) + SHA-256; the auth gate covers every
mutating route; OTA upload requires admin. Plain HTTP only — use on a trusted
LAN. Rotate the admin password after giving anyone access.

## License

GPL-3.0 — see `LICENSE`.
