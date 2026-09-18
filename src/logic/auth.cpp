#include "auth.h"
#include "../common/sha256.h"

namespace ha {

AuthStore::AuthStore() = default;

std::string AuthStore::hash(const std::string& salt,
                            const std::string& password) {
  std::string in = salt + '\x00' + password;
  char hex[65];
  ha_sha256_hex(reinterpret_cast<const uint8_t*>(in.data()), in.size(), hex);
  return std::string(hex);
}

void AuthStore::setAdminPassword(const std::string& password) {
  admin_hash_ = hash(salt_, password);
  admin_must_change_ = false;
}

bool AuthStore::verifyAdmin(const std::string& password) const {
  if (admin_hash_.empty()) return false;
  return hash(salt_, password) == admin_hash_;
}

void AuthStore::setUserEnabled(bool enabled) { user_enabled_ = enabled; }

void AuthStore::setUserPassword(const std::string& password) {
  user_hash_ = hash(salt_, password);
}

bool AuthStore::verifyUser(const std::string& password) const {
  if (!user_enabled_ || user_hash_.empty()) return false;
  return hash(salt_, password) == user_hash_;
}

std::string AuthStore::serialize() const {
  return admin_hash_ + "\n" + (user_enabled_ ? "1" : "0") + "\n" + user_hash_ +
         "\n" + (admin_must_change_ ? "1" : "0") + "\n";
}

bool AuthStore::load(const std::string& data) {
  size_t p1 = data.find('\n');
  if (p1 == std::string::npos) return false;
  size_t p2 = data.find('\n', p1 + 1);
  if (p2 == std::string::npos) return false;
  size_t p3 = data.find('\n', p2 + 1);
  if (p3 == std::string::npos) return false;
  admin_hash_ = data.substr(0, p1);
  user_enabled_ = (data.substr(p1 + 1, p2 - p1 - 1) == "1");
  user_hash_ = data.substr(p2 + 1, p3 - p2 - 1);
  std::string mc = data.substr(p3 + 1);
  if (!mc.empty() && mc.back() == '\n') mc.pop_back();
  admin_must_change_ = (mc != "0");
  if (admin_hash_.size() != 64 && !admin_hash_.empty()) return false;
  return true;
}

}  // namespace ha
