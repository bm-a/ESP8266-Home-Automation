#include "fs_store.h"
#include <LittleFS.h>

bool fsBegin(bool formatOnFail) {
  if (LittleFS.begin()) return true;
  if (!formatOnFail) return false;
  LittleFS.format();
  return LittleFS.begin();
}

std::string fsReadFile(const char* path) {
  File f = LittleFS.open(path, "r");
  if (!f) return std::string();
  std::string out;
  out.reserve(f.size());
  while (f.available()) out += (char)f.read();
  f.close();
  return out;
}

bool fsWriteFile(const char* path, const std::string& data) {
  File f = LittleFS.open(path, "w");
  if (!f) return false;
  size_t n = f.write((const uint8_t*)data.data(), data.size());
  f.close();
  return n == data.size();
}
