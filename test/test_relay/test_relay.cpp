#include <unity.h>
#include "relay.h"
#include "logic/relay.cpp"  // compile implementation into the test binary

using namespace ha;

void setUp() {}
void tearDown() {}

void test_active_low_levels() {
  RelayBank r;
  r.configure(4, true);
  TEST_ASSERT_EQUAL(1, r.levelFor(false));  // OFF = HIGH pin
  TEST_ASSERT_EQUAL(0, r.levelFor(true));   // ON = LOW pin (relay board sinks)
}

void test_active_high_levels() {
  RelayBank r;
  r.configure(4, false);
  TEST_ASSERT_EQUAL(0, r.levelFor(false));
  TEST_ASSERT_EQUAL(1, r.levelFor(true));
}

void test_set_toggle_get() {
  RelayBank r;
  r.configure(4, true);
  TEST_ASSERT_FALSE(r.get(0));
  TEST_ASSERT_TRUE(r.set(0, true));
  TEST_ASSERT_TRUE(r.get(0));
  TEST_ASSERT_EQUAL(0, r.levelForChannel(0));
  TEST_ASSERT_TRUE(r.toggle(0));
  TEST_ASSERT_FALSE(r.get(0));
  TEST_ASSERT_EQUAL(1, r.levelForChannel(0));
}

void test_channels_independent() {
  RelayBank r;
  r.configure(4, true);
  r.set(1, true);
  r.set(3, true);
  TEST_ASSERT_FALSE(r.get(0));
  TEST_ASSERT_TRUE(r.get(1));
  TEST_ASSERT_FALSE(r.get(2));
  TEST_ASSERT_TRUE(r.get(3));
  r.set(1, false);
  TEST_ASSERT_FALSE(r.get(1));
  TEST_ASSERT_TRUE(r.get(3));
}

void test_out_of_range_safe() {
  RelayBank r;
  r.configure(4, true);
  TEST_ASSERT_FALSE(r.set(-1, true));
  TEST_ASSERT_FALSE(r.set(4, true));
  TEST_ASSERT_FALSE(r.toggle(9));
  TEST_ASSERT_FALSE(r.get(4));
}

void test_change_callback() {
  RelayBank r;
  r.configure(2, true);
  int calls = 0, last_ch = -1;
  bool last_on = false;
  r.onChange([&](int ch, bool on) {
    calls++;
    last_ch = ch;
    last_on = on;
  });
  r.set(1, true);
  TEST_ASSERT_EQUAL(1, calls);
  TEST_ASSERT_EQUAL(1, last_ch);
  TEST_ASSERT_TRUE(last_on);
  r.set(1, true);  // no change -> no callback
  TEST_ASSERT_EQUAL(1, calls);
  r.toggle(1);
  TEST_ASSERT_EQUAL(2, calls);
  TEST_ASSERT_FALSE(last_on);
}

void test_all_eight_channels() {
  RelayBank r;
  r.configure(8, true);
  for (int i = 0; i < 8; i++) TEST_ASSERT_TRUE(r.set(i, true));
  for (int i = 0; i < 8; i++) TEST_ASSERT_TRUE(r.get(i));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_active_low_levels);
  RUN_TEST(test_active_high_levels);
  RUN_TEST(test_set_toggle_get);
  RUN_TEST(test_channels_independent);
  RUN_TEST(test_out_of_range_safe);
  RUN_TEST(test_change_callback);
  RUN_TEST(test_all_eight_channels);
  return UNITY_END();
}
