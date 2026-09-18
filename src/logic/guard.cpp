#include "guard.h"

namespace ha {

AttemptTracker::AttemptTracker() = default;

void AttemptTracker::configure(int maxFails, uint32_t windowMs, uint32_t lockMs,
                               int maxIps) {
  maxFails_ = maxFails;
  windowMs_ = windowMs;
  lockMs_ = lockMs;
  maxIps_ = maxIps > 0 ? maxIps : 1;
  entries_.clear();
}

AttemptTracker::Entry* AttemptTracker::find(const std::string& ip,
                                            uint32_t nowMs) {
  for (auto& e : entries_) {
    if (e.ip == ip) {
      // Expired lock + stale window: reset the entry lazily.
      if (e.lockedUntil != 0 && (int32_t)(nowMs - e.lockedUntil) >= 0) {
        e.lockedUntil = 0;
        e.fails = 0;
      }
      return &e;
    }
  }
  return nullptr;
}

bool AttemptTracker::isLocked(const std::string& ip, uint32_t nowMs) {
  Entry* e = find(ip, nowMs);
  if (!e || e->lockedUntil == 0) return false;
  return (int32_t)(nowMs - e->lockedUntil) < 0;
}

bool AttemptTracker::noteFail(const std::string& ip, uint32_t nowMs) {
  Entry* e = find(ip, nowMs);
  if (!e) {
    if ((int)entries_.size() >= maxIps_) entries_.erase(entries_.begin());
    entries_.push_back(Entry{ip, 0, nowMs, 0});
    e = &entries_.back();
  }
  if ((uint32_t)(nowMs - e->windowStart) >= windowMs_) {
    e->windowStart = nowMs;
    e->fails = 0;
  }
  e->fails++;
  if (e->fails >= maxFails_) {
    e->lockedUntil = nowMs + lockMs_;
    return true;
  }
  return false;
}

void AttemptTracker::noteSuccess(const std::string& ip) {
  for (auto it = entries_.begin(); it != entries_.end(); ++it) {
    if (it->ip == ip) {
      entries_.erase(it);
      return;
    }
  }
}

}  // namespace ha
