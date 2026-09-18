// NIST FIPS-180-4 vectors + million-'a' vector for our SHA-256.
#include <cstdio>
#include <string.h>
#include <unity.h>
#include "sha256.h"
#include "common/sha256.cpp"  // compile implementation into the test binary

void setUp() {}
void tearDown() {}

static void hex_of(const char* msg, char out[65]) {
  ha_sha256_hex(reinterpret_cast<const uint8_t*>(msg), strlen(msg), out);
}

void test_empty_string() {
  char out[65];
  hex_of("", out);
  TEST_ASSERT_EQUAL_STRING(
      "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
      out);
}

void test_abc() {
  char out[65];
  hex_of("abc", out);
  TEST_ASSERT_EQUAL_STRING(
      "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
      out);
}

void test_448_bit_boundary() {
  // 56-byte message: exercises the two-block padding path.
  char out[65];
  hex_of("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", out);
  TEST_ASSERT_EQUAL_STRING(
      "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1",
      out);
}

void test_million_a() {
  HA_SHA256_CTX ctx;
  ha_sha256_init(&ctx);
  uint8_t block[1000];
  memset(block, 'a', sizeof(block));
  for (int i = 0; i < 1000; i++) ha_sha256_update(&ctx, block, sizeof(block));
  uint8_t hash[32];
  ha_sha256_final(&ctx, hash);
  char out[65];
  for (int i = 0; i < 32; i++) sprintf(out + i * 2, "%02x", hash[i]);
  TEST_ASSERT_EQUAL_STRING(
      "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0",
      out);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_empty_string);
  RUN_TEST(test_abc);
  RUN_TEST(test_448_bit_boundary);
  RUN_TEST(test_million_a);
  return UNITY_END();
}
