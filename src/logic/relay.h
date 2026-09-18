// Relay bank: logical ON/OFF state decoupled from physical pin level,
// so active-LOW relay modules are handled in one place.
#pragma once
#include <cstdint>
#include <functional>

namespace ha {

class RelayBank {
 public:
  static const int kMaxChannels = 8;

  RelayBank();

  void configure(int channels, bool active_low);
  int channels() const { return channels_; }
  bool activeLow() const { return active_low_; }

  bool set(int ch, bool on);   // returns false if ch out of range
  bool toggle(int ch);
  bool get(int ch) const;      // logical state; false if out of range

  // Physical pin level for a logical state (HIGH=1 / LOW=0).
  int levelFor(bool on) const { return (on != active_low_) ? 1 : 0; }
  int levelForChannel(int ch) const;

  using ChangeCb = std::function<void(int ch, bool on)>;
  void onChange(ChangeCb cb) { change_cb_ = cb; }

 private:
  bool valid(int ch) const { return ch >= 0 && ch < channels_; }
  int channels_ = 0;
  bool active_low_ = true;
  uint8_t state_ = 0;
  ChangeCb change_cb_;
};

}  // namespace ha
