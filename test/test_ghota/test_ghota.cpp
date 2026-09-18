#include <unity.h>
#include "ghota.h"
#include "logic/ghota.cpp"  // compile implementation into the test binary

using namespace ha::ghota;

void setUp() {}
void tearDown() {}

static const char* kReleaseJson =
    "{\"url\":\"https://api.github.com/repos/bm-a/ESP8266-Home-Automation/"
    "releases/1\",\"tag_name\":\"v1.01\",\"name\":\"v1.01\",\"assets\":[{"
    "\"name\":\"README.txt\",\"browser_download_url\":\"https://github.com/"
    "bm-a/x/releases/download/v1.01/README.txt\"},{\"name\":\"esp-home-v1.01."
    "bin\",\"browser_download_url\":\"https://github.com/bm-a/x/releases/"
    "download/v1.01/esp-home-v1.01.bin\"}]}";

void test_parse_tag_and_bin() {
  std::string tag, url;
  TEST_ASSERT_TRUE(parseLatest(kReleaseJson, tag, url));
  TEST_ASSERT_EQUAL_STRING("v1.01", tag.c_str());
  TEST_ASSERT_TRUE(url.find("esp-home-v1.01.bin") != std::string::npos);
}

void test_parse_skips_non_bin() {
  std::string tag, url;
  // README.txt comes first; parser must skip to the .bin asset.
  TEST_ASSERT_TRUE(parseLatest(kReleaseJson, tag, url));
  TEST_ASSERT_TRUE(url.find(".txt") == std::string::npos);
}

void test_parse_no_assets() {
  std::string tag, url;
  TEST_ASSERT_TRUE(parseLatest("{\"tag_name\":\"v2.0\"}", tag, url));
  TEST_ASSERT_EQUAL_STRING("v2.0", tag.c_str());
  TEST_ASSERT_TRUE(url.empty());
}

void test_parse_garbage() {
  std::string tag, url;
  TEST_ASSERT_FALSE(parseLatest("not json at all", tag, url));
  TEST_ASSERT_FALSE(parseLatest("{\"name\":\"x\"}", tag, url));
}

void test_compare_basic() {
  TEST_ASSERT_EQUAL(0, compareVersions("1.0", "1.0"));
  TEST_ASSERT_EQUAL(0, compareVersions("v1.0", "1.0"));
  TEST_ASSERT_EQUAL(-1, compareVersions("1.0", "1.01"));
  TEST_ASSERT_EQUAL(1, compareVersions("1.01", "1.0"));
  TEST_ASSERT_EQUAL(1, compareVersions("2.0", "1.99"));
  TEST_ASSERT_EQUAL(-1, compareVersions("1.9", "1.10"));
}

void test_compare_prerelease() {
  TEST_ASSERT_EQUAL(-1, compareVersions("1.0-beta", "1.0"));
  TEST_ASSERT_EQUAL(1, compareVersions("1.0", "1.0-beta"));
}

void test_update_available() {
  TEST_ASSERT_TRUE(updateAvailable("1.0", "v1.01"));
  TEST_ASSERT_FALSE(updateAvailable("1.01", "v1.01"));
  TEST_ASSERT_FALSE(updateAvailable("1.02", "v1.01"));
  TEST_ASSERT_FALSE(updateAvailable("1.0", ""));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_parse_tag_and_bin);
  RUN_TEST(test_parse_skips_non_bin);
  RUN_TEST(test_parse_no_assets);
  RUN_TEST(test_parse_garbage);
  RUN_TEST(test_compare_basic);
  RUN_TEST(test_compare_prerelease);
  RUN_TEST(test_update_available);
  return UNITY_END();
}
