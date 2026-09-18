#include <algorithm>
#include <unity.h>
#include "auth.h"
#include "common/sha256.cpp"  // sha256 first (auth.cpp needs it linked here)
#include "logic/auth.cpp"     // compile implementation into the test binary

using namespace ha;

void setUp() {}
void tearDown() {}

void test_fresh_store_needs_admin() {
  AuthStore a;
  a.setSalt("testsalt");
  TEST_ASSERT_FALSE(a.adminConfigured());
  TEST_ASSERT_TRUE(a.adminMustChange());
  TEST_ASSERT_FALSE(a.verifyAdmin("admin"));
}

void test_set_and_verify_admin() {
  AuthStore a;
  a.setSalt("testsalt");
  a.setAdminPassword("s3cret!");
  TEST_ASSERT_TRUE(a.adminConfigured());
  TEST_ASSERT_FALSE(a.adminMustChange());
  TEST_ASSERT_TRUE(a.verifyAdmin("s3cret!"));
  TEST_ASSERT_FALSE(a.verifyAdmin("s3cret"));
  TEST_ASSERT_FALSE(a.verifyAdmin(""));
  TEST_ASSERT_FALSE(a.verifyAdmin("admin"));
}

void test_no_plaintext_in_serialized() {
  AuthStore a;
  a.setSalt("testsalt");
  a.setAdminPassword("my-password-123");
  std::string s = a.serialize();
  TEST_ASSERT_TRUE(s.find("my-password-123") == std::string::npos);
  TEST_ASSERT_EQUAL(4, std::count(s.begin(), s.end(), '\n'));
}

void test_serialize_roundtrip() {
  AuthStore a;
  a.setSalt("testsalt");
  a.setAdminPassword("pw1");
  a.setUserEnabled(true);
  a.setUserPassword("pw2");
  AuthStore b;
  b.setSalt("testsalt");
  TEST_ASSERT_TRUE(b.load(a.serialize()));
  TEST_ASSERT_TRUE(b.verifyAdmin("pw1"));
  TEST_ASSERT_TRUE(b.userEnabled());
  TEST_ASSERT_TRUE(b.verifyUser("pw2"));
  TEST_ASSERT_FALSE(b.adminMustChange());
}

void test_user_disabled_by_default() {
  AuthStore a;
  a.setSalt("testsalt");
  a.setUserPassword("user");
  TEST_ASSERT_FALSE(a.verifyUser("user"));  // not enabled yet
  a.setUserEnabled(true);
  TEST_ASSERT_TRUE(a.verifyUser("user"));
  TEST_ASSERT_FALSE(a.verifyUser("USER"));
  a.setUserEnabled(false);
  TEST_ASSERT_FALSE(a.verifyUser("user"));
}

void test_malformed_load_rejected() {
  AuthStore a;
  a.setSalt("testsalt");
  TEST_ASSERT_FALSE(a.load(""));
  TEST_ASSERT_FALSE(a.load("only-one-line"));
  TEST_ASSERT_FALSE(a.load("a\nb\n"));
  TEST_ASSERT_FALSE(a.adminConfigured());
}

void test_salt_changes_hash() {
  AuthStore a, b;
  a.setSalt("salt-A");
  b.setSalt("salt-B");
  a.setAdminPassword("same");
  b.setAdminPassword("same");
  TEST_ASSERT_TRUE(a.serialize() != b.serialize());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_fresh_store_needs_admin);
  RUN_TEST(test_set_and_verify_admin);
  RUN_TEST(test_no_plaintext_in_serialized);
  RUN_TEST(test_serialize_roundtrip);
  RUN_TEST(test_user_disabled_by_default);
  RUN_TEST(test_malformed_load_rejected);
  RUN_TEST(test_salt_changes_hash);
  return UNITY_END();
}
