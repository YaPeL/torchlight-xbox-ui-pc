// The decrypted, decompressed image of a retail XEX2 (Normal/LZX compression): the C++ port of
// tools/xex/xex_dump.py, the same steps and results.
#pragma once
#include <cstdint>

#include "bytes.h"

namespace x360ui {

struct XexImage {
  uint32_t base = 0;
  Bytes image;
};

XexImage xex_dump(const Bytes& xex);

// AES-128 CBC decryption (tiny-AES-c, third_party/tiny-aes): data's length a multiple of 16.
Bytes aes128_cbc_decrypt(const uint8_t key[16], const uint8_t iv[16], Bytes data);

}  // namespace x360ui
