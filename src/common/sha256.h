// Compact public-domain SHA-256 (Brad Conte style, rewritten here).
// Used for salted password hashing. Verified against NIST vectors in tests.
#pragma once
#include <cstddef>
#include <cstdint>

#define HA_SHA256_BLOCK_SIZE 32

typedef struct {
  uint8_t data[64];
  uint32_t datalen;
  uint64_t bitlen;
  uint32_t state[8];
} HA_SHA256_CTX;

void ha_sha256_init(HA_SHA256_CTX *ctx);
void ha_sha256_update(HA_SHA256_CTX *ctx, const uint8_t *data, size_t len);
void ha_sha256_final(HA_SHA256_CTX *ctx, uint8_t hash[HA_SHA256_BLOCK_SIZE]);

// Convenience: hex digest of a byte string.
void ha_sha256_hex(const uint8_t *data, size_t len, char out_hex[65]);
