// Multi-reset recovery counter ("press RESET 3 times").
// Counts consecutive boots inside a time window; the count itself is persisted
// by the device layer (ESP8266 RTC memory survives resets, not power loss).
// Pure logic so host tests can drive it.
#pragma once

namespace ha {

class ResetWindow {
 public:
  ResetWindow();

  void configure(int threshold, unsigned long window_ms);
  void loadCount(int n) { count_ = n < 0 ? 0 : n; }
  int count() const { return count_; }

  // Call once per boot. Returns true when the threshold is reached.
  bool noteBoot() {
    count_++;
    return count_ >= threshold_;
  }

  void clear() { count_ = 0; }

 private:
  int threshold_ = 3;
  unsigned long window_ms_ = 20000;
  int count_ = 0;
};

inline ResetWindow::ResetWindow() = default;

inline void ResetWindow::configure(int threshold, unsigned long window_ms) {
  if (threshold < 2) threshold = 2;
  threshold_ = threshold;
  window_ms_ = window_ms;
  count_ = 0;
}

}  // namespace ha
