#include <unity.h>
#include "logstore.h"
#include "logic/logstore.cpp"  // compile implementation into the test binary

using namespace ha;

void setUp() {}
void tearDown() {}

void test_append_and_dump_roundtrip() {
  LogStore log;
  log.configure(65536, 30);
  log.append(1700000000, "Relay 1 ON");
  log.append(1700000060, "Switch 2 pressed");
  LogStore back;
  back.configure(65536, 30);
  back.load(log.dump());
  TEST_ASSERT_EQUAL(2, back.entries());
  auto lines = back.rendered([](uint32_t e) { return "T" + std::to_string(e); });
  TEST_ASSERT_EQUAL_STRING("T1700000000 Relay 1 ON", lines[0].c_str());
  TEST_ASSERT_EQUAL_STRING("T1700000060 Switch 2 pressed", lines[1].c_str());
}

void test_prune_by_age() {
  LogStore log;
  log.configure(65536, 30);
  uint32_t now = 1700000000;
  log.append(now - 31 * 86400, "too old");
  log.append(now - 29 * 86400, "keeper");
  log.append(now, "fresh");
  log.prune(now);
  TEST_ASSERT_EQUAL(2, log.entries());
  TEST_ASSERT_TRUE(log.dump().find("keeper") != std::string::npos);
  TEST_ASSERT_TRUE(log.dump().find("too old") == std::string::npos);
}

void test_unknown_epoch_kept() {
  LogStore log;
  log.configure(65536, 30);
  log.append(0, "boot before NTP");
  log.prune(1900000000);  // far future must not eat epoch-0 entries
  TEST_ASSERT_EQUAL(1, log.entries());
}

void test_size_cap_drops_oldest() {
  LogStore log;
  log.configure(64, 30);
  for (int i = 0; i < 20; i++) log.append(1700000000 + i, "message number");
  TEST_ASSERT_TRUE(log.bytes() <= 64);
  TEST_ASSERT_TRUE(log.dump().find("message number") != std::string::npos);
  // oldest entries were evicted
  TEST_ASSERT_TRUE(log.dump().find("1700000000 ") == std::string::npos);
}

void test_newlines_sanitized() {
  LogStore log;
  log.configure(65536, 30);
  log.append(1, "evil\n1700000000 forged entry");
  LogStore back;
  back.configure(65536, 30);
  back.load(log.dump());
  TEST_ASSERT_EQUAL(1, back.entries());  // still exactly one entry
}

void test_malformed_lines_skipped() {
  LogStore log;
  log.configure(65536, 30);
  log.load("garbage\n12345\n99999 ok\n");
  TEST_ASSERT_EQUAL(1, log.entries());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_append_and_dump_roundtrip);
  RUN_TEST(test_prune_by_age);
  RUN_TEST(test_unknown_epoch_kept);
  RUN_TEST(test_size_cap_drops_oldest);
  RUN_TEST(test_newlines_sanitized);
  RUN_TEST(test_malformed_lines_skipped);
  return UNITY_END();
}
