// Debounced switch input with edge events and hold-repeat suppression.
// raw=true means "electrically pressed" (device layer inverts pull-ups).
#pragma once
#include <cstdint>

namespace ha {

class SwitchBank {
 public:
  enum Event { NONE, PRESSED, RELEASED };

  static const int kMaxChannels = 8;

  SwitchBank();

  // debounce_ms: settle time. hold_repeat_ms: 0 = suppress repeats while held.
  void configure(int channels, uint32_t debounce_ms, uint32_t hold_repeat_ms = 0);
  void reset();

  // Call every loop with the raw level and now (ms). Returns edge events.
  Event update(int ch, bool raw_pressed, uint32_t now_ms);

  bool pressed(int ch) const;  // stable (debounced) state

 private:
  bool valid(int ch) const { return ch >= 0 && ch < channels_; }
  int channels_ = 0;
  uint32_t debounce_ms_ = 50;
  uint32_t hold_repeat_ms_ = 0;
  bool stable_[kMaxChannels] = {false};
  bool candidate_[kMaxChannels] = {false};
  uint32_t candidate_since_[kMaxChannels] = {0};
  uint32_t last_press_at_[kMaxChannels] = {0};
};

}  // namespace ha
