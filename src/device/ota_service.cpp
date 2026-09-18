#include "ota_service.h"
#include <ArduinoOTA.h>

static std::string s_pending_password;

void otaBegin(const std::string& hostname, const std::string& password) {
  ArduinoOTA.setHostname(hostname.c_str());
  if (!password.empty()) ArduinoOTA.setPassword(password.c_str());
  ArduinoOTA.onStart([]() {});
  ArduinoOTA.onError([](ota_error_t) {});
  ArduinoOTA.begin();
}

void otaLoop() { ArduinoOTA.handle(); }

void otaSetPassword(const std::string& password) {
  s_pending_password = password;  // applied on next boot by main.cpp
  (void)s_pending_password;
}
