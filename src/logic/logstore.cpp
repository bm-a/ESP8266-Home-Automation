#include "logstore.h"

namespace ha {

LogStore::LogStore() = default;

void LogStore::configure(size_t max_bytes, uint32_t max_age_days) {
  max_bytes_ = max_bytes;
  max_age_days_ = max_age_days;
}

void LogStore::load(const std::string& content) {
  entries_.clear();
  size_t pos = 0;
  while (pos < content.size()) {
    size_t eol = content.find('\n', pos);
    std::string line = content.substr(pos, eol == std::string::npos
                                                 ? std::string::npos
                                                 : eol - pos);
    if (eol == std::string::npos)
      pos = content.size();
    else
      pos = eol + 1;
    if (line.empty()) continue;
    size_t sp = line.find(' ');
    if (sp == std::string::npos) continue;
    // No exceptions on embedded targets: validate digits manually.
    uint32_t epoch = 0;
    bool ok = sp > 0;
    for (size_t i = 0; ok && i < sp; i++) {
      char c = line[i];
      if (c < '0' || c > '9') {
        ok = false;
        break;
      }
      epoch = epoch * 10 + (uint32_t)(c - '0');
    }
    if (!ok) continue;
    entries_.push_back(Entry{epoch, line.substr(sp + 1)});
  }
  enforceSize();
}

void LogStore::append(uint32_t epoch, const std::string& message) {
  std::string clean = message;
  for (char& c : clean) {
    if (c == '\n' || c == '\r') c = ' ';
  }
  entries_.push_back(Entry{epoch, clean});
  enforceSize();
}

void LogStore::prune(uint32_t now_epoch) {
  if (max_age_days_ == 0) return;
  uint64_t cutoff = now_epoch > (uint64_t)max_age_days_ * 86400
                        ? now_epoch - (uint64_t)max_age_days_ * 86400
                        : 0;
  // Epoch 0 = "time unknown": keep those (better than deleting blindly).
  std::vector<Entry> kept;
  kept.reserve(entries_.size());
  for (const auto& e : entries_) {
    if (e.epoch == 0 || (uint64_t)e.epoch >= cutoff) kept.push_back(e);
  }
  entries_.swap(kept);
}

std::string LogStore::dump() const {
  std::string out;
  for (const auto& e : entries_) {
    out += std::to_string(e.epoch);
    out += ' ';
    out += e.message;
    out += '\n';
  }
  return out;
}

size_t LogStore::bytes() const { return dump().size(); }

std::vector<std::string> LogStore::rendered(FormatFn fmt) const {
  std::vector<std::string> lines;
  lines.reserve(entries_.size());
  for (const auto& e : entries_) lines.push_back(fmt(e.epoch) + " " + e.message);
  return lines;
}

void LogStore::enforceSize() {
  while (!entries_.empty() && bytes() > max_bytes_) {
    entries_.erase(entries_.begin());  // drop oldest first
  }
}

}  // namespace ha
