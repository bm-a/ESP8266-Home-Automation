#include <unity.h>
#include "guard.h"
#include "logic/guard.cpp"

using namespace ha;

void setUp() {}
void tearDown() {}

void test_lock_after_five() {
  AttemptTracker g;
  g.configure(5, 900000, 300000, 16);
  for (int i = 0; i < 4; i++) {
    TEST_ASSERT_FALSE(g.noteFail("1.2.3.4", 1000 + i));
    TEST_ASSERT_FALSE(g.isLocked("1.2.3.4", 2000));
  }
  TEST_ASSERT_TRUE(g.noteFail("1.2.3.4", 1004));  // 5th triggers
  TEST_ASSERT_TRUE(g.isLocked("1.2.3.4", 2000));
}

void test_lock_expires() {
  AttemptTracker g;
  g.configure(2, 900000, 60000, 16);
  g.noteFail("a", 0);
  g.noteFail("a", 1);
  TEST_ASSERT_TRUE(g.isLocked("a", 30000));
  TEST_ASSERT_FALSE(g.isLocked("a", 61000));  // lock over, count reset
  TEST_ASSERT_FALSE(g.noteFail("a", 61001));  // fresh window
}

void test_window_slides() {
  AttemptTracker g;
  g.configure(3, 1000, 60000, 16);
  g.noteFail("w", 0);
  g.noteFail("w", 100);
  g.noteFail("w", 5000);  // outside 1 s window -> fresh count
  TEST_ASSERT_FALSE(g.isLocked("w", 5001));
}

void test_success_clears() {
  AttemptTracker g;
  g.configure(2, 900000, 60000, 16);
  g.noteFail("s", 0);
  g.noteSuccess("s");
  TEST_ASSERT_FALSE(g.noteFail("s", 1));
  TEST_ASSERT_FALSE(g.isLocked("s", 2));
}

void test_ips_isolated() {
  AttemptTracker g;
  g.configure(2, 900000, 60000, 16);
  g.noteFail("evil", 0);
  g.noteFail("evil", 1);
  TEST_ASSERT_TRUE(g.isLocked("evil", 2));
  TEST_ASSERT_FALSE(g.isLocked("good", 2));
}

void test_other_ips_unaffected_by_eviction() {
  AttemptTracker g;
  g.configure(10, 900000, 60000, 2);  // room for 2 IPs
  g.noteFail("ip1", 0);
  g.noteFail("ip2", 0);
  g.noteFail("ip3", 0);  // evicts ip1
  TEST_ASSERT_FALSE(g.isLocked("ip1", 1));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_lock_after_five);
  RUN_TEST(test_lock_expires);
  RUN_TEST(test_window_slides);
  RUN_TEST(test_success_clears);
  RUN_TEST(test_ips_isolated);
  RUN_TEST(test_other_ips_unaffected_by_eviction);
  return UNITY_END();
}
