#include <unity.h>
#include "debounce.h"
#include "logic/debounce.cpp"  // compile implementation into the test binary

using namespace ha;

void setUp() {}
void tearDown() {}

// Feed a constant raw level for `ms` milliseconds, 1 ms steps.
static int feed(SwitchBank& s, int ch, bool raw, uint32_t& t, uint32_t ms) {
  int presses = 0, releases = 0;
  for (uint32_t i = 0; i < ms; i++) {
    SwitchBank::Event e = s.update(ch, raw, t++);
    if (e == SwitchBank::PRESSED) presses++;
    if (e == SwitchBank::RELEASED) releases++;
  }
  return presses * 10 + releases;  // encode both
}

void test_clean_press_release() {
  SwitchBank s;
  s.configure(4, 50);
  uint32_t t = 1000;
  TEST_ASSERT_EQUAL(10, feed(s, 0, true, t, 100));  // 1 press
  TEST_ASSERT_TRUE(s.pressed(0));
  TEST_ASSERT_EQUAL(1, feed(s, 0, false, t, 100));  // 1 release
  TEST_ASSERT_FALSE(s.pressed(0));
}

void test_bounce_storm_single_edge() {
  SwitchBank s;
  s.configure(4, 50);
  uint32_t t = 0;
  int presses = 0;
  // 200 ms of contact bounce: toggling every 3 ms, then settled pressed.
  for (int i = 0; i < 70; i++) {
    if (s.update(0, (i % 2) == 0, t) == SwitchBank::PRESSED) presses++;
    t += 3;
  }
  for (int i = 0; i < 100; i++) {
    if (s.update(0, true, t++) == SwitchBank::PRESSED) presses++;
  }
  TEST_ASSERT_EQUAL(1, presses);
  TEST_ASSERT_TRUE(s.pressed(0));
}

void test_hold_suppresses_repeats() {
  SwitchBank s;
  s.configure(4, 50, 0);  // 0 = no auto-repeat
  uint32_t t = 0;
  int presses = 0;
  for (uint32_t i = 0; i < 10000; i++) {
    if (s.update(1, true, t++) == SwitchBank::PRESSED) presses++;
  }
  TEST_ASSERT_EQUAL(1, presses);  // 10 s hold -> exactly one event
}

void test_short_glitch_ignored() {
  SwitchBank s;
  s.configure(4, 50);
  uint32_t t = 5000;
  TEST_ASSERT_EQUAL(0, feed(s, 2, true, t, 20));  // 20 ms < 50 ms debounce
  TEST_ASSERT_FALSE(s.pressed(2));
}

void test_channels_independent() {
  SwitchBank s;
  s.configure(4, 50);
  uint32_t t = 0;
  TEST_ASSERT_EQUAL(10, feed(s, 0, true, t, 100));
  TEST_ASSERT_EQUAL(10, feed(s, 3, true, t, 100));
  TEST_ASSERT_TRUE(s.pressed(0));
  TEST_ASSERT_TRUE(s.pressed(3));
  TEST_ASSERT_FALSE(s.pressed(1));
}

void test_out_of_range_safe() {
  SwitchBank s;
  s.configure(4, 50);
  TEST_ASSERT_EQUAL(SwitchBank::NONE, s.update(-1, true, 0));
  TEST_ASSERT_EQUAL(SwitchBank::NONE, s.update(4, true, 0));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_clean_press_release);
  RUN_TEST(test_bounce_storm_single_edge);
  RUN_TEST(test_hold_suppresses_repeats);
  RUN_TEST(test_short_glitch_ignored);
  RUN_TEST(test_channels_independent);
  RUN_TEST(test_out_of_range_safe);
  return UNITY_END();
}
