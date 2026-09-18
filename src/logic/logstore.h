// In-memory event log with age + size rotation.
// Storage format (one line per entry): "<epoch> <message>\n"
// Rendering (human timestamps) is injected so host tests stay libc-only.
#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace ha {

class LogStore {
 public:
  LogStore();

  void configure(size_t max_bytes, uint32_t max_age_days);
  size_t maxBytes() const { return max_bytes_; }

  void load(const std::string& content);  // parse existing file content
  void append(uint32_t epoch, const std::string& message);
  void prune(uint32_t now_epoch);  // drop entries older than max_age_days

  std::string dump() const;  // raw storage format (for writing to flash)
  size_t bytes() const;
  size_t entries() const { return entries_.size(); }

  // Human-readable lines, oldest first.
  using FormatFn = std::function<std::string(uint32_t epoch)>;
  std::vector<std::string> rendered(FormatFn fmt) const;

 private:
  struct Entry {
    uint32_t epoch;
    std::string message;
  };
  void enforceSize();
  size_t max_bytes_ = 65536;
  uint32_t max_age_days_ = 30;
  std::vector<Entry> entries_;
};

}  // namespace ha
