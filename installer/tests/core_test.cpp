// The readers' tests, on synthetic data only (no file of either game).
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include "lzx.h"
#include "stfs.h"
#include "xex.h"

using namespace x360ui;
namespace fs = std::filesystem;

static int failures = 0;
#define CHECK(cond, what)                                    \
  do {                                                       \
    if (!(cond)) {                                           \
      std::printf("FAIL %s (%s:%d)\n", what, __FILE__, __LINE__); \
      failures++;                                            \
    }                                                        \
  } while (0)

static Bytes hex(const char* s) {
  Bytes out;
  for (size_t i = 0; s[i] && s[i + 1]; i += 2) {
    unsigned v;
    std::sscanf(s + i, "%2x", &v);
    out.push_back(uint8_t(v));
  }
  return out;
}

static void put_be32(Bytes& d, size_t o, uint32_t v) {
  d[o] = uint8_t(v >> 24); d[o + 1] = uint8_t(v >> 16); d[o + 2] = uint8_t(v >> 8); d[o + 3] = uint8_t(v);
}

static void test_aes() {
  const Bytes key = hex("000102030405060708090a0b0c0d0e0f");
  const uint8_t zero[16] = {};
  CHECK(aes128_cbc_decrypt(key.data(), zero, hex("69c4e0d86a7b0430d8cdb78070b4c55a")) ==
            hex("00112233445566778899aabbccddeeff"), "FIPS-197 C.1");
  // NIST SP 800-38A F.2.2, the first two blocks.
  const Bytes k2 = hex("2b7e151628aed2a6abf7158809cf4f3c"), iv = hex("000102030405060708090a0b0c0d0e0f");
  CHECK(aes128_cbc_decrypt(k2.data(), iv.data(),
                           hex("7649abac8119b246cee98e9b12e9197d5086cb9b507219ee95db113a917678b2")) ==
            hex("6bc1bee22e409f96e93d7e117393172aae2d8a571e03ac9c9eb76fac45af8e51"), "SP 800-38A F.2.2");
}

// An LZX stream of one uncompressed block: no E8 header bit, type 3, the length, R0-R2, the bytes.
static Bytes lzx_uncompressed(const Bytes& raw) {
  const uint32_t bits = (0u << 31) | (3u << 28) | (uint32_t(raw.size()) << 4);   // 1 + 3 + 24 bits, then 4 of padding
  Bytes s = {uint8_t(bits >> 16), uint8_t(bits >> 24), uint8_t(bits), uint8_t(bits >> 8)};   // 16-bit words, little endian
  for (int r = 0; r < 3; r++) s.insert(s.end(), {1, 0, 0, 0});
  s.insert(s.end(), raw.begin(), raw.end());
  if (raw.size() & 1) s.push_back(0);
  return s;
}

static void test_lzx() {
  Bytes raw;
  for (int i = 0; i < 300; i++) raw.push_back(uint8_t((i * 7) % 200));   // no 0xE8 (the E8 translation)
  CHECK(LzxDecoder(15).decompress(lzx_uncompressed(raw), raw.size()) == raw, "an uncompressed LZX block");
  bool threw = false;
  try { LzxDecoder(14); } catch (const Error&) { threw = true; }
  CHECK(threw, "LZX window bits out of range refused");
}

// A synthetic XEX2: no encryption, Normal compression, one block holding the LZX stream.
static void test_xex() {
  Bytes image = {'M', 'Z'};
  for (int i = 0; i < 200; i++) image.push_back(uint8_t(i % 100));
  const Bytes stream = lzx_uncompressed(image);
  Bytes x(0x400, 0);
  x[0] = 'X'; x[1] = 'E'; x[2] = 'X'; x[3] = '2';
  const uint32_t pe = 0x200, sec = 0x100, ffi = 0x180;
  put_be32(x, 8, pe);
  put_be32(x, 16, sec);
  put_be32(x, 20, 2);                                      // two optional headers
  put_be32(x, 24, 0x000003FF); put_be32(x, 28, ffi);       // file format info
  put_be32(x, 32, 0x000102FF); put_be32(x, 36, 0x82000000);  // image base
  put_be32(x, sec + 4, uint32_t(image.size()));
  x[ffi + 4] = 0; x[ffi + 5] = 0;                          // no encryption
  x[ffi + 6] = 0; x[ffi + 7] = 2;                          // Normal compression
  put_be32(x, ffi + 8, 0x8000);                            // window 2^15
  Bytes block(24, 0);                                      // next size 0, SHA-1 unchecked
  block.push_back(uint8_t(stream.size() >> 8)); block.push_back(uint8_t(stream.size()));
  block.insert(block.end(), stream.begin(), stream.end());
  block.push_back(0); block.push_back(0);
  put_be32(x, ffi + 12, uint32_t(block.size()));
  x.resize(pe);
  x.insert(x.end(), block.begin(), block.end());
  const XexImage out = xex_dump(x);
  CHECK(out.base == 0x82000000, "the XEX's image base");
  CHECK(out.image == image, "the XEX's image");
  bool threw = false;
  try { xex_dump(Bytes(64, 0)); } catch (const Error&) { threw = true; }
  CHECK(threw, "not a XEX2 refused");
}

// The Python tests' stfs_package: a LIVE package of one table copy (header rounded to 0xB000), the
// file table at data block 0, the files after it, chained through the level 0 hash table.
static Bytes stfs_package(const std::vector<std::pair<std::string, Bytes>>& files, bool contiguous,
                          const std::string& parent_folder = "") {
  Bytes out(0xB000, 0);
  std::memcpy(out.data(), "LIVE", 4);
  put_be32(out, 0x340, 0xAD0E);
  put_be32(out, 0x344, 0x000D0000);
  put_be32(out, 0x360, 0x58410A7E);
  const size_t v = 0x379;
  out[v] = 0x24; out[v + 2] = 1; out[v + 3] = 1;
  std::vector<Bytes> blocks{Bytes()};
  std::vector<std::pair<uint32_t, uint32_t>> next;
  Bytes table;
  auto entry = [&](const std::string& name, uint8_t flags, uint32_t count, uint32_t first, int16_t parent, uint32_t size) {
    Bytes e(0x40, 0);
    std::memcpy(e.data(), name.data(), name.size());
    e[0x28] = uint8_t(name.size() | flags);
    e[0x29] = uint8_t(count); e[0x2A] = uint8_t(count >> 8); e[0x2B] = uint8_t(count >> 16);
    e[0x2F] = uint8_t(first); e[0x30] = uint8_t(first >> 8); e[0x31] = uint8_t(first >> 16);
    e[0x32] = uint8_t(uint16_t(parent) >> 8); e[0x33] = uint8_t(parent);
    put_be32(e, 0x34, size);
    table.insert(table.end(), e.begin(), e.end());
  };
  int16_t parent = -1;
  if (!parent_folder.empty()) { entry(parent_folder, 0x80, 0, 0, -1, 0); parent = 0; }
  for (const auto& [name, data] : files) {
    const uint32_t first = uint32_t(blocks.size());
    size_t n = data.empty() ? 1 : (data.size() + 0xFFF) / 0x1000;
    for (size_t k = 0; k < n; k++) blocks.push_back(slice(data, k * 0x1000, 0x1000));
    for (size_t k = 0; k + 1 < n; k++) next.push_back({first + uint32_t(k), first + uint32_t(k) + 1});
    entry(name, contiguous ? 0x40 : 0, uint32_t(n), first, parent, uint32_t(data.size()));
  }
  blocks[0] = table;
  Bytes hash(0x1000, 0);
  for (auto [n, m] : next) { hash[n * 0x18 + 0x15] = uint8_t(m >> 16); hash[n * 0x18 + 0x16] = uint8_t(m >> 8); hash[n * 0x18 + 0x17] = uint8_t(m); }
  out.insert(out.end(), hash.begin(), hash.end());
  for (auto& b : blocks) { b.resize(0x1000, 0); out.insert(out.end(), b.begin(), b.end()); }
  return out;
}

static void test_stfs() {
  const fs::path dir = fs::temp_directory_path() / "x360ui_core_test";
  fs::create_directories(dir);
  Bytes big;
  for (int i = 0; i < 5120; i++) big.push_back(uint8_t(i % 256));
  for (bool contiguous : {true, false}) {
    const fs::path pkg = dir / "pkg";
    {
      std::ofstream f(pkg, std::ios::binary);
      const Bytes p = stfs_package({{"pak.zip", Bytes{'z', 'i', 'p'}}, {"default.xex", big}}, contiguous, "folder");
      f.write(reinterpret_cast<const char*>(p.data()), std::streamsize(p.size()));
    }
    StfsPackage p(pkg.string());
    CHECK(p.title_id == 0x58410A7E, "the package's title");
    CHECK(p.files.size() == 2 && p.files[0].path == "folder/pak.zip", "the files' paths under their folder");
    const StfsEntry* e = p.find("FOLDER/DEFAULT.XEX");
    CHECK(e && p.read(*e) == big, contiguous ? "a contiguous file read" : "a chained file read");
    const std::string out = p.extract(*p.find("folder/pak.zip"), (dir / "out").string());
    std::ifstream f(out, std::ios::binary);
    CHECK(std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>()) == "zip", "a file extracted");
  }
  {
    const fs::path bad = dir / "bad";
    std::ofstream(bad, std::ios::binary) << "MZ" << std::string(0x400, '\0');
    bool threw = false;
    try { StfsPackage p(bad.string()); } catch (const Error&) { threw = true; }
    CHECK(threw, "not a package refused");
  }
  fs::remove_all(dir);
}

static void test_lower() {
  // Latin-1 C0 DE D7 5A, kept as UTF-8: lowered as Python lowers "\u00c0\u00de\u00d7Z".
  CHECK(lower_latin1("\xC3\x80\xC3\x9E\xC3\x97Z") == "\xC3\xA0\xC3\xBE\xC3\x97z", "Latin-1 names lowered as Python");
}

int main() {
  test_lower();
  test_aes();
  test_lzx();
  test_xex();
  test_stfs();
  std::printf(failures ? "%d failed\n" : "all passed\n", failures);
  return failures ? 1 : 0;
}
