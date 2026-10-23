// Bounds-checked reads of big and little endian integers from a byte buffer.
#pragma once
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace x360ui {

using Bytes = std::vector<uint8_t>;

struct Error : std::runtime_error {
  using std::runtime_error::runtime_error;
};

inline void need(size_t size, size_t off, size_t n) {
  if (off > size || n > size - off) throw Error("read past the end of the data");
}

inline uint32_t be32(const Bytes& d, size_t o) {
  need(d.size(), o, 4);
  return uint32_t(d[o]) << 24 | uint32_t(d[o + 1]) << 16 | uint32_t(d[o + 2]) << 8 | d[o + 3];
}

inline uint16_t be16(const Bytes& d, size_t o) {
  need(d.size(), o, 2);
  return uint16_t(d[o] << 8 | d[o + 1]);
}

inline uint32_t be24(const Bytes& d, size_t o) {
  need(d.size(), o, 3);
  return uint32_t(d[o]) << 16 | uint32_t(d[o + 1]) << 8 | d[o + 2];
}

inline uint32_t le24(const Bytes& d, size_t o) {
  need(d.size(), o, 3);
  return uint32_t(d[o]) | uint32_t(d[o + 1]) << 8 | uint32_t(d[o + 2]) << 16;
}

inline uint16_t le16(const Bytes& d, size_t o) {
  need(d.size(), o, 2);
  return uint16_t(d[o] | d[o + 1] << 8);
}

// The bytes [o, o + n) of d, fewer at its end (a slice, as Python's d[o:o + n]).
inline Bytes slice(const Bytes& d, size_t o, size_t n) {
  if (o >= d.size()) return {};
  size_t end = n > d.size() - o ? d.size() : o + n;
  return Bytes(d.begin() + o, d.begin() + end);
}

}  // namespace x360ui
