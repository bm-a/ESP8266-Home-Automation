// Rollover-safe periodic timer (unsigned subtraction handles millis() wrap).
#pragma once
#include <cstdint>

namespace ha {

class Every {
 public:
  explicit Every(uint32_t interval_ms = 1000) : interval_(interval_ms) {}
  void setInterval(uint32_t ms) { interval_ = ms; }
  void reset(uint32_t now_ms) { last_ = now_ms; armed_ = true; }
  // True once per interval. First call after construction fires immediately.
  bool due(uint32_t now_ms) {
    if (!armed_) {
      armed_ = true;
      last_ = now_ms;
      return true;
    }
    if ((uint32_t)(now_ms - last_) >= interval_) {
      last_ = now_ms;
      return true;
    }
    return false;
  }

 private:
  uint32_t interval_;
  uint32_t last_ = 0;
  bool armed_ = false;
};

}  // namespace ha
