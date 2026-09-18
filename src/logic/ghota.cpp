#include "ghota.h"
#include <cctype>
#include <vector>

namespace ha {
namespace ghota {

namespace {
std::string trim(const std::string& s) {
  size_t a = 0;
  while (a < s.size() && isspace((unsigned char)s[a])) a++;
  size_t b = s.size();
  while (b > a && isspace((unsigned char)s[b - 1])) b--;
  std::string t = s.substr(a, b - a);
  if (!t.empty() && (t[0] == 'v' || t[0] == 'V')) t = t.substr(1);
  return t;
}

// Find "key" : "value" with optional whitespace. Returns false if absent.
bool jsonString(const std::string& json, const std::string& key,
                std::string& out, size_t from = 0) {
  std::string q = "\"" + key + "\"";
  size_t k = json.find(q, from);
  if (k == std::string::npos) return false;
  size_t c = json.find(':', k + q.size());
  if (c == std::string::npos) return false;
  size_t q1 = json.find('"', c + 1);
  if (q1 == std::string::npos) return false;
  size_t q2 = json.find('"', q1 + 1);
  if (q2 == std::string::npos) return false;
  out = json.substr(q1 + 1, q2 - q1 - 1);
  return true;
}
}  // namespace

bool parseLatest(const std::string& json, std::string& tagOut,
                 std::string& binUrlOut) {
  tagOut.clear();
  binUrlOut.clear();
  if (!jsonString(json, "tag_name", tagOut)) return false;
  // First asset whose download URL ends in .bin (query strings stripped).
  size_t from = 0;
  std::string url;
  while (jsonString(json, "browser_download_url", url, from)) {
    size_t end = url.find_first_of("?#");
    std::string path = url.substr(0, end);
    if (path.size() >= 4 &&
        path.compare(path.size() - 4, 4, ".bin") == 0) {
      binUrlOut = url;
      break;
    }
    from = json.find(url, from) + url.size();
    if (from == std::string::npos) break;
  }
  return true;
}

int compareVersions(const std::string& a, const std::string& b) {
  std::string ta = trim(a), tb = trim(b);
  size_t ia = 0, ib = 0;
  while (ia < ta.size() || ib < tb.size()) {
    // Parse one numeric component from each side.
    long na = 0, nb = 0;
    bool aPre = false, bPre = false;
    while (ia < ta.size() && ta[ia] != '.') {
      char c = ta[ia++];
      if (isdigit((unsigned char)c))
        na = na * 10 + (c - '0');
      else if (c == '-' || !(na == 0 && (c == ' ')))
        aPre = true;  // pre-release suffix devalues the component
    }
    while (ib < tb.size() && tb[ib] != '.') {
      char c = tb[ib++];
      if (isdigit((unsigned char)c))
        nb = nb * 10 + (c - '0');
      else
        bPre = true;
    }
    if (ia < ta.size() && ta[ia] == '.') ia++;
    if (ib < tb.size() && tb[ib] == '.') ib++;
    if (na != nb) return na < nb ? -1 : 1;
    if (aPre != bPre) return aPre ? -1 : 1;  // release beats pre-release
  }
  return 0;
}

}  // namespace ghota
}  // namespace ha
