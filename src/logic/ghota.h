// GitHub-release OTA helpers (hardware-independent, host-tested).
//
// Parses the (already downloaded) GitHub "latest release" JSON for the tag
// name and the first .bin asset URL, and compares dotted version numbers.
// The device layer only fetches bytes; all decisions live here.
#pragma once
#include <string>

namespace ha {
namespace ghota {

// Extract tag_name and first browser_download_url ending in ".bin".
// Returns true if at least the tag was found (bin URL may stay empty).
bool parseLatest(const std::string& json, std::string& tagOut,
                 std::string& binUrlOut);

// Compare dotted versions ("1.01" vs "1.0"): -1 / 0 / +1.
// Leading 'v'/'V' and surrounding whitespace are ignored; non-numeric
// suffixes ("-beta") make that component compare lower.
int compareVersions(const std::string& a, const std::string& b);

// True if `latest` is newer than `current`.
inline bool updateAvailable(const std::string& current,
                            const std::string& latest) {
  if (latest.empty()) return false;
  return compareVersions(latest, current) > 0;
}

}  // namespace ghota
}  // namespace ha
