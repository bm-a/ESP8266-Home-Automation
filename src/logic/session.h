// Server-side session store (RAM only): login returns an opaque token the
// client keeps in an HttpOnly SameSite cookie. Passwords (Basic auth) are
// then never replayed per request, and CSRF dies via SameSite + Origin check.
// Pure logic: time and randomness are injected for host tests.
#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace ha {

const int kRoleUser = 1;
const int kRoleAdmin = 2;

class SessionStore {
 public:
  SessionStore();

  using RngFn = std::function<uint32_t()>;  // 32 random bits per call
  void configure(int maxSessions, uint32_t ttlMs);
  void setRng(RngFn rng) { rng_ = rng; }

  // Returns hex token ("" when full). role is kRoleUser/kRoleAdmin.
  std::string login(int role, uint32_t nowMs);
  // Returns role, or -1. Slides expiry on success.
  int validate(const std::string& token, uint32_t nowMs);
  void logout(const std::string& token);
  void sweep(uint32_t nowMs);  // drop expired
  int active() const;

 private:
  struct Session {
    std::string token;
    int role;
    uint32_t expiresAt;
  };
  static std::string hexToken(RngFn rng);
  int maxSessions_ = 8;
  uint32_t ttlMs_ = 1800000;  // 30 min
  RngFn rng_;
  std::vector<Session> sessions_;
};

}  // namespace ha
