// Credential store: salted SHA-256 hashes, admin forced-change on first boot,
// optional limited "user" account. No plaintext passwords anywhere.
#pragma once
#include <string>

namespace ha {

class AuthStore {
 public:
  AuthStore();

  // Salt should be device-unique (e.g. chip id hex). Tests use a fixed salt.
  void setSalt(const std::string& salt) { salt_ = salt; }

  bool adminConfigured() const { return !admin_hash_.empty(); }
  bool adminMustChange() const { return admin_must_change_; }
  void setAdminPassword(const std::string& password);  // clears must-change
  bool verifyAdmin(const std::string& password) const;

  void setUserEnabled(bool enabled);
  bool userEnabled() const { return user_enabled_; }
  void setUserPassword(const std::string& password);
  bool verifyUser(const std::string& password) const;

  // Persistable form: "admin_hash user_enabled user_hash must_change"
  std::string serialize() const;
  bool load(const std::string& data);

 private:
  static std::string hash(const std::string& salt, const std::string& password);
  std::string salt_ = "ha-default-salt";
  std::string admin_hash_;
  bool admin_must_change_ = true;
  bool user_enabled_ = false;
  std::string user_hash_;
};

}  // namespace ha
