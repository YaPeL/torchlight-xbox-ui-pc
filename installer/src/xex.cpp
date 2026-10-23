#include "xex.h"

#include "lzx.h"

extern "C" {
#include "aes.h"
}

namespace x360ui {
namespace {
const uint8_t kRetailKey[16] = {0x20, 0xB1, 0x85, 0xA5, 0x9D, 0x28, 0xFD, 0xC3,
                                0x40, 0x58, 0x3F, 0xBB, 0x08, 0x96, 0xBF, 0x91};
const uint8_t kBlankIv[16] = {};
constexpr uint32_t kSecAesKeyOffset = 0x150, kSecImageSizeOffset = 0x4;
constexpr uint32_t kOptFileFormatInfo = 0x000003, kOptImageBase = 0x000102;
constexpr uint16_t kEncryptionNormal = 1, kCompressionNormal = 2;
}  // namespace

Bytes aes128_cbc_decrypt(const uint8_t key[16], const uint8_t iv[16], Bytes data) {
  if (data.size() % 16) throw Error("CBC data must be whole 16 byte blocks");
  AES_ctx ctx;
  AES_init_ctx_iv(&ctx, key, iv);
  AES_CBC_decrypt_buffer(&ctx, data.data(), data.size());
  return data;
}

XexImage xex_dump(const Bytes& xex) {
  if (xex.size() < 4 || xex[0] != 'X' || xex[1] != 'E' || xex[2] != 'X' || xex[3] != '2')
    throw Error("not a XEX2 file");
  const uint32_t pe_offset = be32(xex, 8), sec_info = be32(xex, 16);
  uint32_t ffi = 0, base = 0;
  for (uint32_t i = 0, n = be32(xex, 20); i < n; i++) {
    const uint32_t key = be32(xex, 24 + size_t(i) * 8), value = be32(xex, 28 + size_t(i) * 8);
    if (key >> 8 == kOptFileFormatInfo)
      ffi = value;
    else if (key >> 8 == kOptImageBase)
      base = value;
  }
  if (be16(xex, size_t(ffi) + 6) != kCompressionNormal) throw Error("only Normal (LZX) compression is supported");
  const uint32_t image_size = be32(xex, size_t(sec_info) + kSecImageSizeOffset);
  Bytes data = slice(xex, pe_offset, xex.size());
  data.resize(data.size() + (16 - data.size() % 16) % 16, 0);
  if (be16(xex, size_t(ffi) + 4) == kEncryptionNormal) {
    Bytes enc_key = slice(xex, size_t(sec_info) + kSecAesKeyOffset, 16);
    if (enc_key.size() != 16) throw Error("read past the end of the data");
    const Bytes file_key = aes128_cbc_decrypt(kRetailKey, kBlankIv, enc_key);
    data = aes128_cbc_decrypt(file_key.data(), kBlankIv, std::move(data));
  }
  // The block chain is in the data: each block starts with {next block's size (4), its SHA-1
  // (20)} then {u16 size, bytes} chunks up to a size of 0; they make one LZX stream.
  const uint32_t window_size = be32(xex, size_t(ffi) + 8);
  uint32_t block_size = be32(xex, size_t(ffi) + 12);
  Bytes stream;
  size_t pos = 0;
  while (block_size) {
    const Bytes block = slice(data, pos, block_size);
    const uint32_t next_size = be32(block, 0);
    size_t q = 24;
    while (true) {
      const uint16_t chunk = be16(block, q);
      q += 2;
      if (chunk == 0) break;
      const Bytes part = slice(block, q, chunk);
      stream.insert(stream.end(), part.begin(), part.end());
      q += chunk;
    }
    pos += block_size;
    block_size = next_size;
  }
  int window_bits = -1;
  for (uint32_t w = window_size; w; w >>= 1) window_bits++;   // bit_length() - 1
  XexImage out;
  out.base = base;
  out.image = LzxDecoder(window_bits).decompress(stream, image_size);
  return out;
}

}  // namespace x360ui
