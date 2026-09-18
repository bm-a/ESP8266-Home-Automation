// Clock abstraction: device uses ::millis(), tests inject a manual clock.
#pragma once
#include <cstdint>

namespace ha {

class IClock {
 public:
  virtual ~IClock() = default;
  virtual uint32_t millis() const = 0;
  virtual uint32_t epoch() const = 0;  // seconds since 1970, 0 = unknown
};

}  // namespace ha
