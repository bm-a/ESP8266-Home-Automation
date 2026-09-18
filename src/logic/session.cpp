#include "session.h"

namespace ha {

SessionStore::SessionStore() {
  // Default RNG is weak (test override required on device too).
  rng_ = []() -> uint32_t {
    static uint32_t s = 0x9E3779B9u;
    s = s * 1664525u + 1013904223u;
    return s;
  };
}

void SessionStore::configure(int maxSessions, uint32_t ttlMs) {
  if (maxSessions < 1) maxSessions = 1;
  maxSessions_ = maxSessions;
  ttlMs_ = ttlMs;
  sessions_.clear();
}

std::string SessionStore::hexToken(RngFn rng) {
  static const char* d = "0123456789abcdef";
  std::string t;
  t.reserve(32);
  for (int i = 0; i < 4; i++) {
    uint32_t r = rng();
    for (int n = 0; n < 8; n++) t += d[(r >> (n * 4)) & 0xF];
  }
  return t;
}

std::string SessionStore::login(int role, uint32_t nowMs) {
  if (role != kRoleUser && role != kRoleAdmin) return std::string();
  sweep(nowMs);
  if ((int)sessions_.size() >= maxSessions_) return std::string();
  Session s{hexToken(rng_), role, nowMs + ttlMs_};
  sessions_.push_back(s);
  return s.token;
}

int SessionStore::validate(const std::string& token, uint32_t nowMs) {
  if (token.empty()) return -1;
  for (auto& s : sessions_) {
    if (s.token == token) {
      if ((int32_t)(nowMs - s.expiresAt) >= 0) return -1;  // expired
      s.expiresAt = nowMs + ttlMs_;                        // slide
      return s.role;
    }
  }
  return -1;
}

void SessionStore::logout(const std::string& token) {
  for (auto it = sessions_.begin(); it != sessions_.end(); ++it) {
    if (it->token == token) {
      sessions_.erase(it);
      return;
    }
  }
}

void SessionStore::sweep(uint32_t nowMs) {
  std::vector<Session> kept;
  for (auto& s : sessions_) {
    if ((int32_t)(nowMs - s.expiresAt) < 0) kept.push_back(s);
  }
  sessions_.swap(kept);
}

int SessionStore::active() const { return (int)sessions_.size(); }

}  // namespace ha
