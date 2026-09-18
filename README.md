# ESP8266 Home Automation — 4-Channel Relay Controller with WiFi, Web Interface and OTA

Control four AC/DC loads (lights, fans, pumps, garage doors) from your phone or
computer, or from physical wall switches — using a $3 ESP8266 board and relay
module. No cloud account, no app install, no subscription: the board hosts its
own web page on your home WiFi.

**Keywords:** ESP8266 home automation, WiFi relay control, ESP8266 4-channel
relay, NodeMCU relay web server, ESP8266 captive portal WiFi setup, ESP8266 OTA
firmware update, Arduino relay controller with manual switches.

---

## Table of contents

1. [Features — what it does and how](#1-features--what-it-does-and-how)
2. [What you need (parts list)](#2-what-you-need-parts-list)
3. [Wiring — step by step](#3-wiring--step-by-step)
4. [Installing the firmware (3 ways)](#4-installing-the-firmware-3-ways)
5. [First boot — connecting it to your WiFi](#5-first-boot--connecting-it-to-your-wifi)
6. [Daily use](#6-daily-use)
7. [Settings reference](#7-settings-reference)
8. [HTTP API reference](#8-http-api-reference)
9. [Event log](#9-event-log)
10. [Firmware updates (OTA)](#10-firmware-updates-ota)
11. [Troubleshooting](#11-troubleshooting)
12. [FAQ](#12-faq)
13. [Technical specifications](#13-technical-specifications)
14. [For developers](#14-for-developers)
15. [Security notes](#15-security-notes)
16. [License](#16-license)

---

## 1. Features — what it does and how

### 1.1 Web dashboard (phone / PC, no app)

The board runs its own website. Open its address in any browser and you get
four ON/OFF buttons with live state (refreshed every 2 seconds), plus clock,
signal strength and IP. Works on Android, iPhone, desktop — anything with a
browser. No internet required after setup; everything stays on your LAN.

### 1.2 Physical switches (manual override that always works)

Four wall-switch / push-button inputs toggle the four relays directly on the
board — even if WiFi is down, the router is off, or the firmware is busy.
Inputs are **debounced in software (50 ms)** so one press = exactly one toggle
(no flicker), and holding a button never spams repeats. Every press is written
to the event log with a timestamp.

### 1.3 Captive-portal WiFi setup (no hardcoding passwords)

You never edit code to enter your WiFi password. On first boot (or whenever it
can't reach your router) the board opens its own hotspot named
`ESP-Setup-XXXX`. Join it and a setup page appears automatically, where you
pick your home network and type the password. Credentials are stored in flash.
If the network is unreachable, the board keeps running standalone — relays and
switches work, and it retries in the background. **Hold switch 1 for 8 seconds**
any time to reopen the portal (5-minute window) and move the device to a new
network.

### 1.4 Two user roles (admin + limited user)

- **admin** — full access: relays, logs, passwords, WiFi portal, reboot, firmware updates.
- **user** (optional, disabled by default) — can only see state and toggle relays. Ideal for family members or tenants.

There are **no factory default passwords**. On first boot the site forces you
to create the admin password (minimum 8 characters) before anything else works.

### 1.5 Event log with real timestamps

Every relay change (web or physical), login-relevant change, boot, WiFi and OTA
event is recorded with date and time. The admin views it at `/logs`. Time comes
from an NTP server (`in.pool.ntp.org`, India timezone +5:30) with a forced sync
at boot, so timestamps are correct from the first minute. Old entries are
rotated automatically (30 days / 64 KB cap) and flash writes are throttled so
the memory isn't worn out.

### 1.6 Relay state memory (survives power cuts)

Relay states are saved to flash on every change and restored at boot. A power
cut at night doesn't leave your porch light logic inverted in the morning.

### 1.7 Wireless firmware updates (OTA), two ways

- **Browser upload:** Settings → OTA page, pick the `.bin`, upload. Admin-only.
- **ArduinoOTA:** push directly from PlatformIO/Arduino IDE over the network.
  Both are password-protected.

### 1.8 Keep-alive for strict hotspots

Some mobile hotspots and hotel/college networks kick idle devices. Every
15 minutes the board sends a tiny probe to keep its connection warm, and logs
it if the probe fails so you know the network dropped — not the board.

### 1.9 Local discovery (mDNS)

Reach the board at `esp-home.local` instead of hunting for its IP in the
router page (works on most phones/desktops; falls back to the IP shown in the
dashboard).

---

## 2. What you need (parts list)

| # | Part | Notes |
|---|---|---|
| 1 | ESP8266 dev board | ESP-12E, NodeMCU, or Wemos D1 mini — any 4 MB flash module |
| 2 | 4-channel 5 V relay module | The common optocou isolated type (active-LOW trigger) |
| 3 | 4× push buttons or wall switches | Momentary, normally-open, wired to GND |
| 4 | 1× 10 kΩ resistor | Pull-up for switch 2 (GPIO16 has no internal pull-up) |
| 5 | 5 V power supply | 1–2 A phone charger style; powers relays + board via USB |
| 6 | Jumper wires | — |

Total cost is typically under $10 / ₹800 excluding mains wiring accessories.

> **Mains warning:** relay modules switch real 230 V/110 V AC. If you are not
> comfortable with mains wiring, have an electrician do the high-voltage side.
> Keep AC traces physically separated from the low-voltage board.

---

## 3. Wiring — step by step

### 3.1 Relay outputs

| Relay | ESP8266 GPIO | Board label (NodeMCU) | Relay module pin |
|---|---|---|---|
| 1 | GPIO5 | D1 | IN1 |
| 2 | GPIO4 | D2 | IN2 |
| 3 | GPIO12 | D6 | IN3 |
| 4 | GPIO13 | D7 | IN4 |

Relay module VCC → 5 V, GND → GND (shared with ESP ground). If your module
triggers on HIGH instead of LOW, flip the `RELAY_ACTIVE_LOW` flag (PlatformIO
`platformio.ini`, or the define at the top of the Arduino sketch) and re-flash.

### 3.2 Switch inputs (buttons to GND)

| Switch | ESP8266 GPIO | Board label | Wiring |
|---|---|---|---|
| 1 | GPIO14 | D5 | button between pin and GND |
| 2 | GPIO16 | D0 | button between pin and GND **+ 10 kΩ resistor from pin to 3V3** |
| 3 | GPIO0 | FLASH btn | button between pin and GND |
| 4 | GPIO2 | D4 | button between pin and GND |

Pressing a button connects its pin to ground — that is the "pressed" signal.
Switches 1, 3 and 4 use the chip's internal pull-ups (configured in firmware);
switch 2 needs the external 10 kΩ resistor because GPIO16 has no internal one.

### 3.3 Why these pins

GPIO1/3 are the USB serial port (needed for flashing and debug), GPIO6–11 talk
to the flash chip, and GPIO15 decides the boot mode — touching any of them
causes boot failures or kills flashing. The chosen pins avoid all of that.
GPIO0/2 are strapping pins but are safe with buttons-to-GND (exactly how the
board's own FLASH button is wired). The onboard LED shares GPIO2; firmware only
ever reads that pin, never drives it.

---

## 4. Installing the firmware (3 ways)

### Option A — prebuilt binary (easiest, 2 minutes)

1. Download `esp-home-v1.0.bin` from the
   [Releases page](https://github.com/bm-a/ESP8266-Home-Automation/releases).
2. Install esptool: `pip install esptool`.
3. Hold FLASH, plug in USB, run:

```sh
esptool.py --port /dev/ttyUSB0 write_flash 0x00000 esp-home-v1.0.bin
```

(Windows: port looks like `COM3`.)

### Option B — PlatformIO (recommended for tweaking)

1. Install [PlatformIO](https://platformio.org/) (VS Code extension or `pip install platformio`).
2. Clone this repo, open it, run `pio run -e esp12e -t upload`.
3. Optional checks: `pio test -e native` (34 unit tests), `sh run_tests.sh` (tests + 30-day simulator).

### Option C — Arduino IDE

1. Add the ESP8266 core in Boards Manager, install libraries **WiFiManager**
   (tzapu), **NTPClient**, **Time** (PaulStoffregen) via Library Manager.
2. Open `arduino/ESP8266_Home_Automation/ESP8266_Home_Automation.ino`
   (generated from the same sources — never edit it by hand; edit `src/` and
   run `python3 tools/mirror_to_ino.py`).
3. Board: "Generic ESP8266 Module", Flash Size 4M, FS: LittleFS. Upload.

---

## 5. First boot — connecting it to your WiFi

1. Power the board. It creates a hotspot called **`ESP-Setup-XXXX`** (last 4
   hex digits of the chip ID — printed on the serial monitor at 115200 baud).
2. Join that hotspot on your phone. The setup page should pop up automatically;
   if not, open `http://192.168.4.1`.
3. Select your home WiFi, enter the password, save. The board reboots and
   connects (portal gives up after 5 minutes and the board runs standalone).
4. Find it: check your router's client list, or try `http://esp-home.local`.
5. The site redirects to **first-time setup**: create the admin password
   (≥ 8 chars). Done — bookmark the page.

To change networks later: hold **switch 1 for 8 seconds**, or Settings →
"Open WiFi portal" (admin).

---

## 6. Daily use

- **Toggle a load:** open the dashboard, press Turn ON / Turn OFF. Or press the
  physical switch — both stay in sync because there is one relay state.
- **Check state at a glance:** the dashboard polls every 2 s; the header shows
  time, IP and WiFi signal (RSSI in dBm; above −70 is healthy).
- **Give someone limited access:** admin → Settings → enable the `user`
  account with its own password. That login sees relays only — no logs, no
  settings, no updates.
- **After a power cut:** relays return to their last state automatically.

---

## 7. Settings reference

| Page / control | Who | What it does |
|---|---|---|
| `/` dashboard | user+ | Relay buttons, clock, IP, RSSI |
| `/logs` | admin | Timestamped event history |
| `/settings` → Admin password | admin | Change admin login (≥ 8 chars, applies immediately) |
| `/settings` → User account | admin | Enable/disable `user`, set its password (≥ 4 chars) |
| `/settings` → OTA password | admin | Password for ArduinoOTA + browser upload (applies after reboot) |
| `/settings` → Open WiFi portal | admin | Reopens `ESP-Setup-XXXX` for 5 min |
| `/settings` → Reboot | admin | Clean reboot (state is saved first) |
| `/update` | admin | Browser firmware upload form |

---

## 8. HTTP API reference

Base: `http://<board-ip>` (or `http://esp-home.local`). Authentication: HTTP
Basic (`admin` or `user`). `user` may call state + relay endpoints only.

| Method + path | Auth | Description |
|---|---|---|
| `GET /api/state` | user | `{"time":"[…]","ip":"…","rssi":-58,"ntp":true,"uptime":1234,"relays":[true,false,false,true]}` |
| `POST /api/relay` | user | Form fields `ch=0..3`, `on=1/0` (omit `on` to toggle). Returns `{"ch":0,"on":true}` |
| `POST /relay/<i>` | user | Legacy shape: toggles relay `i` |
| `GET /api/logs` | admin | Plain-text log, oldest first |
| `POST /api/setup` | — (first boot only) | `password=…` (≥ 8 chars) unlocks the device |
| `POST /api/adminpw` | admin | `password=…` change admin password |
| `POST /api/userset` | admin | `enabled=1/0` + `password=…` |
| `POST /api/otapw` | admin | `password=…` (needs reboot) |
| `POST /api/portal` | admin | Open WiFi portal |
| `POST /api/restart` | admin | Reboot |

Example (toggle relay 1 with curl):

```sh
curl -u user:mypass -X POST http://esp-home.local/api/relay --data "ch=0"
```

---

## 9. Event log

Stored in flash (`/log.txt`), capped at 64 KB / 30 days with oldest-first
rotation. Entries look like:

```
[2026-09-19 08:31:02] Switch 1 pressed -> Relay 1 ON
[2026-09-19 08:31:09] Relay 1 OFF (web)
[2026-09-19 08:32:00] NTP synced
```

Logged: relay changes (source tagged `web` or switch number), boots, WiFi
connect/drop, NTP syncs, password changes, OTA results, portal openings,
keepalive failures. Entries from before the first NTP sync keep epoch `0` and
are never auto-deleted by rotation (shown as `[no-time]`).

---

## 10. Firmware updates (OTA)

**Browser:** `/update` (admin) → choose the new `.bin` → Upload. The board
verifies, flashes, logs the result and reboots. Only signed-in admin sessions
can start an upload; anything else gets `403`.

**ArduinoOTA:** the board advertises as `esp-home`. In PlatformIO set
`upload_protocol = espota` + `upload_port = esp-home.local`, using the OTA
password from Settings. Changing the OTA password needs one reboot to apply.

---

## 11. Troubleshooting

| Symptom | Likely cause → fix |
|---|---|
| `ESP-Setup-XXXX` never appears | Board didn't boot: check USB power, re-flash; watch serial @115200 |
| Setup page doesn't pop up | Open `http://192.168.4.1` manually; disable mobile data |
| Board won't join home WiFi | 2.4 GHz only (ESP8266 can't see 5 GHz); check password; move closer |
| Relays work inverted | Your module is active-HIGH: set `RELAY_ACTIVE_LOW 0` and re-flash |
| Relay clicks but load stays off | Check COM/NO/NC wiring on the module; separate 5 V supply for 4 relays |
| Switch 2 never registers | Missing 10 kΩ pull-up to 3V3 on GPIO16 |
| Switch needs many presses | Mechanical bounce beyond 50 ms: increase `HA_DEBOUNCE_MS` in `src/config.h` |
| Dashboard shows `[no-time]` | No internet for NTP; relays/switches unaffected; time fills in when online |
| `esp-home.local` doesn't resolve | mDNS blocked by router/VPN: use the IP from the dashboard header or router list |
| OTA upload fails | `.bin` built for the wrong board/flash size; reboot and retry |
| Forgot admin password | Re-flash with USB (erases settings), or hold switch-1 portal trick won't help — USB reflash is the recovery path |

---

## 12. FAQ

**Does it need internet?** Only for clock sync and remote… there is no remote:
control is LAN-only by design. Relays and switches work with no network at all.

**ESP8266 or ESP32?** This project targets the cheaper ESP8266 (plenty for 4
relays). The web/API design ports directly to ESP32 if you ever outgrow it.

**How many loads?** Four, matching common 4-channel relay boards. The code
supports up to 8 (`HA_CHANNELS`, tested to 8 in the unit suite) if you add
wiring and free GPIOs.

**Will rapid switching wear the flash?** Log writes are batched (every 25
events or 60 s) and relay state is one byte — years of normal use.

**Can I use it for mains appliances?** Yes — that is what relay modules are
for — but have an electrician wire the AC side, use a proper enclosure, fuse
the supply, and never expose terminals.

---

## 13. Technical specifications

| Item | Value |
|---|---|
| MCU | ESP8266 (ESP-12E / NodeMCU / D1 mini), 80 MHz |
| Firmware size | ~432 KB flash (41%), ~37 KB RAM (46%) |
| Web server | Port 80, HTTP Basic auth, JSON API |
| Time | NTP `in.pool.ntp.org`, +5:30, 5-min resync, boot force-sync |
| Log | LittleFS `/log.txt`, 64 KB / 30-day rotation |
| WiFi | STA + captive portal (`ESP-Setup-XXXX`, 5-min timeout), mDNS `esp-home.local` |
| OTA | HTTP upload + ArduinoOTA, password-protected |
| Switches | 50 ms debounce, edge-triggered, 8 s hold = portal |
| Relays | Active-LOW default, state persisted, restored at boot |

---

## 14. For developers

```
src/logic/    hardware-independent core (RelayBank, SwitchBank, LogStore,
              AuthStore, scheduler) — covered by host unit tests
src/device/   ESP8266 drivers: portal, web UI, OTA, NTP, LittleFS storage
src/common/   shared SHA-256 (password hashing)
src/main.cpp  wiring + loop
test/         6 Unity suites, 34 assertions (run: pio test -e native)
tools/soak_sim.cpp      30-day simulator: switch storms, WiFi drops,
                        millis() wrap (run: sh run_tests.sh)
tools/mirror_to_ino.py  regenerates arduino/ from src/ (CI-enforced fresh)
tools/check_ino.sh      compile-checks the generated sketch
arduino/      single-file sketch for Arduino IDE (generated, do not edit)
firmware/     prebuilt esp-home-v1.0.bin + flashing notes
legacy/       original v3 sketch, untouched, for reference
```

The v3→v1.0 rewrite fixed: dead `/relay/<index>` route, TODO WiFi/hotspot
handlers, never-started hotspot, boot-hang on empty SSID, broken OTA upload,
`timestamp[20]` overflow, no-op log pruning, undebounced SPIFFS spam, relay on
the TX pin, hardcoded admin/admin. See `CHANGELOG.md`.

---

## 15. Security notes

- No default credentials; first-boot setup gate blocks all mutating routes.
- Passwords stored salted (per-chip salt) + SHA-256 — never plaintext.
- Plain HTTP only: safe on a trusted home LAN, not for port-forwarding to the
  internet. Put it behind VPN (e.g. Tailscale) for remote access.
- If the admin password leaks, change it in Settings and rotate the OTA
  password too.

## 16. License

GPL-3.0 — see `LICENSE`. The original v3 sketch this was rewritten from
carries the same license.
