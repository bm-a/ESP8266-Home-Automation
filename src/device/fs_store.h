// LittleFS persistence helpers (device only).
#pragma once
#include <string>

bool fsBegin(bool formatOnFail = true);
std::string fsReadFile(const char* path);
bool fsWriteFile(const char* path, const std::string& data);
