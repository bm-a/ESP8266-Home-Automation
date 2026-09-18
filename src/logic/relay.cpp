#include "relay.h"

namespace ha {

RelayBank::RelayBank() = default;

void RelayBank::configure(int channels, bool active_low) {
  if (channels < 0) channels = 0;
  if (channels > kMaxChannels) channels = kMaxChannels;
  channels_ = channels;
  active_low_ = active_low;
  state_ = 0;
}

bool RelayBank::set(int ch, bool on) {
  if (!valid(ch)) return false;
  bool prev = get(ch);
  if (on)
    state_ |= (uint8_t)(1u << ch);
  else
    state_ &= (uint8_t)(~(1u << ch));
  if (on != prev && change_cb_) change_cb_(ch, on);
  return true;
}

bool RelayBank::toggle(int ch) {
  if (!valid(ch)) return false;
  return set(ch, !get(ch));
}

bool RelayBank::get(int ch) const {
  if (!valid(ch)) return false;
  return (state_ & (1u << ch)) != 0;
}

int RelayBank::levelForChannel(int ch) const { return levelFor(get(ch)); }

}  // namespace ha
