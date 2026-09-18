// NTP time sync (device only). TimeLib holds wall-clock for formatting.
#pragma once
#include <cstdint>
#include <string>

void timeSyncBegin();
bool timeSyncForceUpdate(int retries = 5);  // blocking, call at boot
void timeSyncLoop();                        // call every loop
uint32_t timeEpoch();                       // 0 = unknown
bool timeIsValid();
std::string timeFormat(uint32_t epoch);  // "[YYYY-MM-DD HH:MM:SS]", 24B-safe
