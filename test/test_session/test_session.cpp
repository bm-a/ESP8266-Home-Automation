#include <unity.h>
#include "session.h"
#include "logic/session.cpp"

using namespace ha;

static uint32_t s_rng = 0xA5A5A5A5u;
static uint32_t testRng() {
  s_rng = s_rng * 1664525u + 1013904223u;
  return s_rng;
}

void setUp() { s_rng = 0xA5A5A5A5u; }
void tearDown() {}

void test_login_validate_logout() {
  SessionStore s;
  s.configure(8, 1800000);
  s.setRng(testRng);
  std::string t = s.login(kRoleAdmin, 1000);
  TEST_ASSERT_EQUAL(32, t.size());
  TEST_ASSERT_EQUAL(kRoleAdmin, s.validate(t, 2000));
  s.logout(t);
  TEST_ASSERT_EQUAL(-1, s.validate(t, 3000));
}

void test_unknown_token_rejected() {
  SessionStore s;
  s.configure(8, 1800000);
  s.setRng(testRng);
  TEST_ASSERT_EQUAL(-1, s.validate("nope", 0));
  TEST_ASSERT_EQUAL(-1, s.validate("", 0));
}

void test_expiry_and_slide() {
  SessionStore s;
  s.configure(8, 1000);  // 1 s TTL
  s.setRng(testRng);
  std::string t = s.login(kRoleUser, 0);
  TEST_ASSERT_EQUAL(kRoleUser, s.validate(t, 500));  // slides to 1500
  TEST_ASSERT_EQUAL(kRoleUser, s.validate(t, 1400));
  TEST_ASSERT_EQUAL(-1, s.validate(t, 2500));  // expired
}

void test_expiry_rollover_safe() {
  SessionStore s;
  s.configure(8, 1000);
  s.setRng(testRng);
  std::string t = s.login(kRoleUser, 0xFFFFFF00u);
  TEST_ASSERT_EQUAL(kRoleUser, s.validate(t, 0xFFFFFF00u + 500u));
  TEST_ASSERT_EQUAL(-1, s.validate(t, 0xFFFFFF00u + 2000u));
}

void test_capacity_cap() {
  SessionStore s;
  s.configure(2, 60000);
  s.setRng(testRng);
  TEST_ASSERT_FALSE(s.login(kRoleUser, 0).empty());
  TEST_ASSERT_FALSE(s.login(kRoleUser, 0).empty());
  TEST_ASSERT_TRUE(s.login(kRoleUser, 0).empty());  // full
  TEST_ASSERT_EQUAL(2, s.active());
}

void test_tokens_unique() {
  SessionStore s;
  s.configure(8, 60000);
  s.setRng(testRng);
  std::string a = s.login(kRoleUser, 0);
  std::string b = s.login(kRoleUser, 0);
  TEST_ASSERT_TRUE(a != b);
}

void test_bad_role_rejected() {
  SessionStore s;
  s.configure(8, 60000);
  s.setRng(testRng);
  TEST_ASSERT_TRUE(s.login(0, 0).empty());
  TEST_ASSERT_TRUE(s.login(99, 0).empty());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_login_validate_logout);
  RUN_TEST(test_unknown_token_rejected);
  RUN_TEST(test_expiry_and_slide);
  RUN_TEST(test_expiry_rollover_safe);
  RUN_TEST(test_capacity_cap);
  RUN_TEST(test_tokens_unique);
  RUN_TEST(test_bad_role_rejected);
  return UNITY_END();
}
