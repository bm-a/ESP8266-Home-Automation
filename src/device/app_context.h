// Shared application context passed to device modules.
#pragma once
#include <functional>
#include <string>
#include "../logic/relay.h"
#include "../logic/debounce.h"
#include "../logic/logstore.h"
#include "../logic/auth.h"

struct AppContext {
  ha::RelayBank* relays = nullptr;
  ha::SwitchBank* switches = nullptr;
  ha::LogStore* log = nullptr;
  ha::AuthStore* auth = nullptr;

  const int* relayPins = nullptr;
  const int* switchPins = nullptr;
  int channels = 0;

  // Time / status supplied by main.cpp.
  std::function<uint32_t()> epochNow;
  std::function<std::string(uint32_t)> fmtTime;
  std::function<void(const std::string&)> addLog;  // append + schedule flush
  std::function<void()> saveAll;                   // persist relays/auth/log
  std::function<void()> requestPortal;             // open WiFi portal ASAP
  std::function<void()> requestReboot;

  bool* wifiUp = nullptr;
  bool* ntpOk = nullptr;
  std::string* otaPassword = nullptr;  // ArduinoOTA/HTTP-upload password
  const char* fwVersion = nullptr;     // HA_VERSION, shown on /update
};
