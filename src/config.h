// Hardware + behavior configuration for v1.0.
//
// PIN MAP (generic ESP8266 dev module / ESP-12E / NodeMCU):
//   Relays  D1 D2 D6 D7 = GPIO 5,4,12,13  (no strapping, no UART, no flash bus)
//   Switches D5 D0 + FLASH btn + D4 LED   = GPIO 14,16,0,2 (buttons to GND)
// Avoided: GPIO1(TX) 3(RX) 6-11(flash) 15(strapping).
// GPIO16 has NO internal pull-up: fit an external 10 kΩ resistor to 3V3.
// GPIO0/2 use INPUT_PULLUP + button-to-GND (same as the FLASH button: safe).
#pragma once

#define HA_CHANNELS 4

#define HA_VERSION "1.01"

// Triple-reset recovery: this many RESET presses inside the window below
// wipes WiFi credentials and opens the config portal.
#define HA_RESET_COUNT 3
#define HA_RESET_WINDOW_MS 20000UL

// Re-attempt WiFi association after this long without a connection.
#define HA_RECONNECT_AFTER_MS 60000UL

#define HA_RELAY_PINS \
  { 5, 4, 12, 13 }
#define HA_SWITCH_PINS \
  { 14, 16, 0, 2 }

#ifndef RELAY_ACTIVE_LOW
#define RELAY_ACTIVE_LOW 1  // common relay modules trigger on LOW
#endif

#define HA_DEBOUNCE_MS 50
#define HA_PORTAL_TRIGGER_HOLD_MS 8000  // hold switch 1 to open WiFi portal
#define HA_PORTAL_TIMEOUT_S 300
#define HA_NTP_SERVER "in.pool.ntp.org"
#define HA_NTP_OFFSET_S 19800  // Asia/Kolkata +5:30
#define HA_NTP_INTERVAL_MS 300000UL
#define HA_LOG_FILE "/log.txt"
#define HA_LOG_MAX_BYTES 65536
#define HA_LOG_MAX_AGE_DAYS 30
#define HA_AUTH_FILE "/auth.dat"
#define HA_RELAY_FILE "/relays.dat"
#define HA_OTAKEY_FILE "/otakey.txt"
#define HA_KEEPALIVE_INTERVAL_MS 900000UL  // 15 min anti-idle probe
#define HA_LOG_FLUSH_INTERVAL_MS 60000UL
#define HA_LOG_FLUSH_EVERY_N 25
