// OTA: ArduinoOTA (IDE uploads) + authenticated HTTP upload (device only).
#pragma once
#include <string>

void otaBegin(const std::string& hostname, const std::string& password);
void otaLoop();
// Apply a new password (takes effect after reboot).
void otaSetPassword(const std::string& password);
