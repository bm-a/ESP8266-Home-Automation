#include <unity.h>
#include "resetwin.h"

using namespace ha;

void setUp() {}
void tearDown() {}

void test_threshold_three() {
  ResetWindow w;
  w.configure(3, 20000);
  TEST_ASSERT_FALSE(w.noteBoot());  // 1st
  TEST_ASSERT_FALSE(w.noteBoot());  // 2nd
  TEST_ASSERT_TRUE(w.noteBoot());   // 3rd -> recover
  TEST_ASSERT_TRUE(w.noteBoot());   // stays triggered until cleared
}

void test_clear_resets() {
  ResetWindow w;
  w.configure(3, 20000);
  w.noteBoot();
  w.noteBoot();
  w.clear();
  TEST_ASSERT_EQUAL(0, w.count());
  TEST_ASSERT_FALSE(w.noteBoot());
}

void test_load_persisted_count() {
  ResetWindow w;
  w.configure(3, 20000);
  w.loadCount(2);  // two quick resets already happened (from RTC memory)
  TEST_ASSERT_TRUE(w.noteBoot());
}

void test_custom_threshold() {
  ResetWindow w;
  w.configure(2, 10000);  // double-reset variant
  TEST_ASSERT_FALSE(w.noteBoot());
  TEST_ASSERT_TRUE(w.noteBoot());
}

void test_negative_load_clamped() {
  ResetWindow w;
  w.configure(3, 20000);
  w.loadCount(-5);
  TEST_ASSERT_EQUAL(0, w.count());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_threshold_three);
  RUN_TEST(test_clear_resets);
  RUN_TEST(test_load_persisted_count);
  RUN_TEST(test_custom_threshold);
  RUN_TEST(test_negative_load_clamped);
  return UNITY_END();
}
