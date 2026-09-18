#include "debounce.h"

namespace ha {

SwitchBank::SwitchBank() = default;

void SwitchBank::configure(int channels, uint32_t debounce_ms,
                           uint32_t hold_repeat_ms) {
  if (channels < 0) channels = 0;
  if (channels > kMaxChannels) channels = kMaxChannels;
  channels_ = channels;
  debounce_ms_ = debounce_ms;
  hold_repeat_ms_ = hold_repeat_ms;
  reset();
}

void SwitchBank::reset() {
  for (int i = 0; i < kMaxChannels; ++i) {
    stable_[i] = false;
    candidate_[i] = false;
    candidate_since_[i] = 0;
    last_press_at_[i] = 0;
  }
}

SwitchBank::Event SwitchBank::update(int ch, bool raw_pressed, uint32_t now_ms) {
  if (!valid(ch)) return NONE;
  if (raw_pressed != candidate_[ch]) {
    candidate_[ch] = raw_pressed;
    candidate_since_[ch] = now_ms;
    return NONE;
  }
  if (raw_pressed == stable_[ch]) return NONE;
  if ((uint32_t)(now_ms - candidate_since_[ch]) < debounce_ms_) return NONE;
  // Settled into a new stable state.
  stable_[ch] = raw_pressed;
  if (raw_pressed) {
    last_press_at_[ch] = now_ms;
    return PRESSED;
  }
  return RELEASED;
}

bool SwitchBank::pressed(int ch) const {
  if (!valid(ch)) return false;
  return stable_[ch];
}

}  // namespace ha
