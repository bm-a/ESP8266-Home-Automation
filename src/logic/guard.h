// Brute-force guard: per-IP failure counting with sliding window + lockout.
// Pure logic; the device keys it by client IP, tests use fake addresses.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace ha {

class AttemptTracker {
 public:
  AttemptTracker();

  void configure(int maxFails, uint32_t windowMs, uint32_t lockMs, int maxIps);
  bool isLocked(const std::string& ip, uint32_t nowMs);
  // Returns true if this failure (just) triggered a lock.
  bool noteFail(const std::string& ip, uint32_t nowMs);
  void noteSuccess(const std::string& ip);

 private:
  struct Entry {
    std::string ip;
    int fails = 0;
    uint32_t windowStart = 0;
    uint32_t lockedUntil = 0;
  };
  Entry* find(const std::string& ip, uint32_t nowMs);
  int maxFails_ = 5;
  uint32_t windowMs_ = 900000;   // 15 min
  uint32_t lockMs_ = 300000;     // 5 min
  int maxIps_ = 16;
  std::vector<Entry> entries_;
};

}  // namespace ha
