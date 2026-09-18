#include <unity.h>
#include "scheduler.h"

using namespace ha;

void setUp() {}
void tearDown() {}

void test_fires_immediately_first_time() {
  Every e(1000);
  TEST_ASSERT_TRUE(e.due(5000));
  TEST_ASSERT_FALSE(e.due(5000));
}

void test_periodic() {
  Every e(1000);
  e.reset(0);
  TEST_ASSERT_FALSE(e.due(999));
  TEST_ASSERT_TRUE(e.due(1000));
  TEST_ASSERT_FALSE(e.due(1500));
  TEST_ASSERT_TRUE(e.due(2000));
}

void test_millis_rollover() {
  Every e(1000);
  e.reset(0xFFFFFF00u);  // near 32-bit wrap
  TEST_ASSERT_FALSE(e.due(0xFFFFFF00u + 500u));   // wrapped to 244, only 500 elapsed
  TEST_ASSERT_TRUE(e.due(0xFFFFFF00u + 1500u));   // wrapped to 1244, 1500 elapsed
}

void test_back_to_back_intervals() {
  Every e(100);
  e.reset(0);
  int fires = 0;
  for (uint32_t t = 0; t <= 1000; t++) {
    if (e.due(t)) fires++;
  }
  TEST_ASSERT_EQUAL(10, fires);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_fires_immediately_first_time);
  RUN_TEST(test_periodic);
  RUN_TEST(test_millis_rollover);
  RUN_TEST(test_back_to_back_intervals);
  return UNITY_END();
}
