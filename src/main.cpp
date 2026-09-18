// ESP8266 Home Automation v1.0 — 4 relays + 4 switches, captive-portal WiFi,
// authenticated web UI + JSON API, event log, NTP, OTA. See README.md.
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiClient.h>

#include "config.h"
#include "device/app_context.h"
#include "device/fs_store.h"
#include "device/gh_update.h"
#include "device/ota_service.h"
#include "device/reset_recovery.h"
#include "device/time_sync.h"
#include "device/web_ui.h"
#include "device/wifi_portal.h"
#include "logic/ghota.h"
#include "logic/resetwin.h"
#include "logic/scheduler.h"

static const char* kDeviceName = DEVICE_NAME;

static const int kRelayPins[HA_CHANNELS] = HA_RELAY_PINS;
static const int kSwitchPins[HA_CHANNELS] = HA_SWITCH_PINS;

static ha::RelayBank s_relays;
static ha::SwitchBank s_switches;
static ha::LogStore s_log;
static ha::AuthStore s_auth;
static ha::ResetWindow s_resetWin;
static AppContext s_ctx;

static bool s_wifiUp = false;
static bool s_ntpOk = false;
static std::string s_otaPassword;
static int s_logPending = 0;
static bool s_portalRequested = false;
static bool s_rebootRequested = false;
static uint32_t s_sw0PressSince = 0;
static bool s_portalFiredForPress = false;

static ha::Every s_ntpTick(HA_NTP_INTERVAL_MS);
static ha::Every s_flushTick(HA_LOG_FLUSH_INTERVAL_MS);
static ha::Every s_pruneTick(86400000UL);
static ha::Every s_keepaliveTick(HA_KEEPALIVE_INTERVAL_MS);
static ha::Every s_wifiTick(5000);
static uint32_t s_downSince = 0;
static bool s_windowExpired = false;
static bool s_ghBootChecked = false;

static void persistAll() {
  uint8_t mask = 0;
  for (int i = 0; i < HA_CHANNELS; i++)
    if (s_relays.get(i)) mask |= (uint8_t)(1u << i);
  std::string r;
  r += (char)mask;
  fsWriteFile(HA_RELAY_FILE, r);
  fsWriteFile(HA_AUTH_FILE, s_auth.serialize());
  fsWriteFile(HA_LOG_FILE, s_log.dump());
  fsWriteFile(HA_OTAKEY_FILE, s_otaPassword);
}

static void addLog(const std::string& msg) {
  s_log.append(timeEpoch(), msg);
  s_logPending++;
}

static void applyRelayPin(int ch) {
  digitalWrite(kRelayPins[ch], s_relays.levelForChannel(ch));
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("ESP-Home " HA_VERSION " booting");

  if (!fsBegin(true)) Serial.println("FS mount failed!");

  // Unique salt per device so identical passwords hash differently.
  char salt[16];
  snprintf(salt, sizeof(salt), "esp-%08X", ESP.getChipId());
  s_auth.setSalt(salt);
  if (!s_auth.load(fsReadFile(HA_AUTH_FILE)))
    Serial.println("No auth store; first-boot setup required");

  s_log.configure(HA_LOG_MAX_BYTES, HA_LOG_MAX_AGE_DAYS);
  s_log.load(fsReadFile(HA_LOG_FILE));

  std::string ota = fsReadFile(HA_OTAKEY_FILE);
  if (!ota.empty() && ota.back() == '\n') ota.pop_back();
  s_otaPassword = ota;

  // Relays: pins first, then restore saved states.
  s_relays.configure(HA_CHANNELS, RELAY_ACTIVE_LOW != 0);
  for (int i = 0; i < HA_CHANNELS; i++) {
    pinMode(kRelayPins[i], OUTPUT);
    applyRelayPin(i);  // safe OFF before restore
  }
  std::string saved = fsReadFile(HA_RELAY_FILE);
  if (saved.size() == 1) {
    for (int i = 0; i < HA_CHANNELS; i++)
      if (saved[0] & (1 << i)) {
        s_relays.set(i, true);
        applyRelayPin(i);
      }
  }
  s_relays.onChange([](int ch, bool) {
    applyRelayPin(ch);
    uint8_t mask = 0;
    for (int i = 0; i < HA_CHANNELS; i++)
      if (s_relays.get(i)) mask |= (uint8_t)(1u << i);
    std::string r;
    r += (char)mask;
    fsWriteFile(HA_RELAY_FILE, r);  // persist on every change
  });

  // Switches: buttons to GND. GPIO16 needs its external 10k pull-up.
  s_switches.configure(HA_CHANNELS, HA_DEBOUNCE_MS, 0);
  for (int i = 0; i < HA_CHANNELS; i++) {
    if (kSwitchPins[i] == 16)
      pinMode(kSwitchPins[i], INPUT);
    else
      pinMode(kSwitchPins[i], INPUT_PULLUP);
  }

  // Context for the web layer.
  s_ctx.relays = &s_relays;
  s_ctx.switches = &s_switches;
  s_ctx.log = &s_log;
  s_ctx.auth = &s_auth;
  s_ctx.relayPins = kRelayPins;
  s_ctx.switchPins = kSwitchPins;
  s_ctx.channels = HA_CHANNELS;
  s_ctx.epochNow = []() { return timeEpoch(); };
  s_ctx.fmtTime = [](uint32_t e) { return timeFormat(e); };
  s_ctx.addLog = addLog;
  s_ctx.saveAll = persistAll;
  s_ctx.requestPortal = []() { s_portalRequested = true; };
  s_ctx.requestReboot = []() { s_rebootRequested = true; };
  s_ctx.wifiUp = &s_wifiUp;
  s_ctx.ntpOk = &s_ntpOk;
  s_ctx.otaPassword = &s_otaPassword;
  s_ctx.fwVersion = HA_VERSION;

  // Triple-reset recovery: N quick RESET presses -> wipe WiFi + portal.
  // The count lives in RTC memory: resets preserve it, power loss clears it.
  s_resetWin.configure(HA_RESET_COUNT, HA_RESET_WINDOW_MS);
  s_resetWin.loadCount(resetRecoveryCount());
  bool tripleReset = s_resetWin.noteBoot();
  resetRecoverySave(s_resetWin.count());

  // Network: portal fallback, then time, OTA, mDNS, web.
  if (tripleReset) {
    addLog("Triple-reset: WiFi erased, opening portal");
    Serial.println("Triple-reset recovery: erasing WiFi");
    WiFi.disconnect(true);
    wifiPortalOpen("ESP-Setup", HA_PORTAL_TIMEOUT_S);
    wifiPortalPoll();
    s_wifiUp = wifiIsUp();
    resetRecoveryClear();
    s_resetWin.clear();
    s_windowExpired = true;
  } else {
    s_wifiUp = wifiPortalBoot("ESP-Setup", HA_PORTAL_TIMEOUT_S);
  }
  Serial.print("WiFi: ");
  Serial.println(s_wifiUp ? wifiIp().c_str() : "offline");
  addLog(s_wifiUp ? "WiFi connected" : "WiFi offline; running standalone");

  timeSyncBegin();
  if (s_wifiUp && timeSyncForceUpdate()) {
    s_ntpOk = true;
    addLog("NTP synced");
  }
  s_log.prune(timeEpoch());

  otaBegin(kDeviceName, s_otaPassword);
  MDNS.begin(kDeviceName);
  webBegin(&s_ctx);
  Serial.println("Ready");
  addLog("Boot complete");
  persistAll();
}

static void pollSwitches() {
  uint32_t now = millis();
  for (int i = 0; i < HA_CHANNELS; i++) {
    bool raw = (digitalRead(kSwitchPins[i]) == LOW);  // pressed = GND
    ha::SwitchBank::Event e = s_switches.update(i, raw, now);
    if (e == ha::SwitchBank::PRESSED) {
      bool on = !s_relays.get(i);
      s_relays.set(i, on);  // callback drives pin + persists
      char msg[48];
      snprintf(msg, sizeof(msg), "Switch %d pressed -> Relay %d %s", i + 1,
               i + 1, on ? "ON" : "OFF");
      addLog(msg);
      if (i == 0) {
        s_sw0PressSince = now;
        s_portalFiredForPress = false;
      }
    }
    if (e == ha::SwitchBank::RELEASED && i == 0) s_sw0PressSince = 0;
  }
  // Hold switch 1 for 8 s -> open the WiFi config portal.
  if (s_sw0PressSince != 0 && !s_portalFiredForPress &&
      (uint32_t)(now - s_sw0PressSince) >= HA_PORTAL_TRIGGER_HOLD_MS) {
    s_portalFiredForPress = true;
    s_portalRequested = true;
    addLog("Portal requested (switch hold)");
  }
}

static void keepalive() {
  // Some hotspots drop idle clients: a brief TCP probe keeps the lease warm.
  WiFiClient c;
  if (!c.connect("google.com", 80)) {
    addLog("Keepalive probe failed");
    return;
  }
  c.print("HEAD / HTTP/1.0\r\nHost: google.com\r\n\r\n");
  c.stop();
}

void loop() {
  webLoop();
  otaLoop();
  MDNS.update();

  uint32_t now = millis();
  if (s_wifiTick.due(now)) {
    bool wasUp = s_wifiUp;
    wifiPortalPoll();
    s_wifiUp = wifiIsUp();
    if (s_wifiUp) {
      s_downSince = 0;
    } else {
      // Network-drop recovery: re-associate (rate-limited). No auto-reboot:
      // a dead router doesn't need one, and reboots blink the relays.
      if (wasUp) s_downSince = now;
      if (WiFi.SSID().length() > 0 &&
          (uint32_t)(now - s_downSince) >= HA_RECONNECT_AFTER_MS) {
        s_downSince = now;
        addLog("WiFi down 60s: reconnecting");
        WiFi.reconnect();
      }
    }
  }
  if (s_ntpTick.due(now) && s_wifiUp) {
    timeSyncLoop();
    s_ntpOk = timeIsValid();
  }
  if (s_pruneTick.due(now)) {
    s_log.prune(timeEpoch());
    persistAll();
  }
  if (s_keepaliveTick.due(now) && s_wifiUp) keepalive();
  if (s_flushTick.due(now) && s_logPending > 0) {
    fsWriteFile(HA_LOG_FILE, s_log.dump());
    s_logPending = 0;
  } else if (s_logPending >= HA_LOG_FLUSH_EVERY_N) {
    fsWriteFile(HA_LOG_FILE, s_log.dump());
    s_logPending = 0;
  }

  pollSwitches();

  // Reset window expired: slow boots are not button mashing.
  if (!s_windowExpired && now >= HA_RESET_WINDOW_MS) {
    s_windowExpired = true;
    resetRecoveryClear();
    s_resetWin.clear();
  }

  // One-shot GitHub update check shortly after boot (then on demand).
  if (!s_ghBootChecked && now >= 60000 && s_wifiUp) {
    s_ghBootChecked = true;
    std::string tag, url;
    if (ghCheckLatest(tag, url) &&
        ha::ghota::updateAvailable(HA_VERSION, tag)) {
      addLog("Update available: " + tag + " (see /update)");
    }
  }

  if (s_portalRequested) {
    s_portalRequested = false;
    addLog("Opening config portal");
    wifiPortalOpen("ESP-Setup", HA_PORTAL_TIMEOUT_S);
    s_wifiUp = wifiIsUp();
  }
  if (s_rebootRequested) {
    persistAll();
    resetRecoveryDisarm();  // intentional reboot, not a button press
    delay(300);
    ESP.restart();
  }
}
