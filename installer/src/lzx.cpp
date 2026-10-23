// The port of tools/xex/lzx_decompress.py (MIT License, Copyright (c) 2026 sp00nznet), derived
// from libmspack's LZX decoder ((C) 2003-2023 Stuart Caie, originally LGPL-2.1; under GPL-3.0 per
// its section 3: installer/third_party/NOTICES.md): names and
// steps kept, Python's unbounded integers replaced by 64-bit ones (the bit buffer never holds more
// than 39 bits: 24 asked plus a 16-bit word less one).
#include "lzx.h"

#include <algorithm>

namespace x360ui {
namespace {
constexpr int kNumChars = 256, kMinMatch = 2, kNumPrimaryLengths = 7, kSecondaryNumElements = 249;
constexpr int kPretreeNum = 20, kPretreeTablebits = 6, kPretreeMaxsymbols = 20, kPretreeMaxCodeword = 16;
constexpr int kMaintreeTablebits = 11, kMaintreeMaxsymbols = kNumChars + (51 << 3), kMaintreeMaxCodeword = 16;
constexpr int kLentreeTablebits = 10, kLentreeMaxsymbols = kSecondaryNumElements, kLentreeMaxCodeword = 16;
constexpr int kAligntreeTablebits = 7, kAligntreeMaxsymbols = 8, kAligntreeMaxCodeword = 8;
constexpr int kLentableSafety = 64;
constexpr uint32_t kVerbatim = 1, kAligned = 2, kUncompressed = 3;

const uint32_t kPositionBase[] = {
    0, 1, 2, 3, 4, 6, 8, 12, 16, 24, 32, 48, 64, 96, 128, 192,
    256, 384, 512, 768, 1024, 1536, 2048, 3072, 4096, 6144, 8192,
    12288, 16384, 24576, 32768, 49152, 65536, 98304, 131072, 196608,
    262144, 393216, 524288, 655360, 786432, 917504, 1048576, 1179648,
    1310720, 1441792, 1572864, 1703936, 1835008, 1966080, 2097152,
};
const uint8_t kExtraBits[] = {
    0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
    7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13, 14, 14,
    15, 15, 16, 16, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17,
    17, 17, 17, 17,
};

uint32_t position_slots(int bits) {
  switch (bits) {
    case 15: return 30;
    case 16: return 32;
    case 17: return 34;
    case 18: return 36;
    case 19: return 38;
    case 20: return 42;
    case 21: return 50;
  }
  throw Error("window_bits must be 15-21, got " + std::to_string(bits));
}

template <class T>
T& at(std::vector<T>& v, int64_t i) {  // Python's indexing: out of range is an error
  if (i < 0 || uint64_t(i) >= v.size()) throw Error("LZX: a table index out of range (bad data)");
  return v[size_t(i)];
}

// _make_decode_table
bool make_decode_table(int nsyms, int nbits, std::vector<uint8_t>& length, std::vector<uint16_t>& table) {
  uint32_t pos = 0, table_mask = 1u << nbits, bit_mask = table_mask >> 1, next_symbol = bit_mask;
  for (int bit_num = 1; bit_num <= nbits; bit_num++) {
    for (int sym = 0; sym < nsyms; sym++) {
      if (at(length, sym) == bit_num) {
        uint32_t leaf = pos;
        pos += bit_mask;
        if (pos > table_mask) return false;
        for (uint32_t k = 0; k < bit_mask; k++) at(table, leaf + k) = uint16_t(sym);
      }
    }
    bit_mask >>= 1;
  }
  if (pos == table_mask) return true;
  for (uint32_t sym = pos; sym < table_mask; sym++) at(table, sym) = 0;
  uint64_t lpos = uint64_t(pos) << 16, lmask = uint64_t(table_mask) << 16, lbit = 1u << 15;
  for (int bit_num = nbits + 1; bit_num <= 16; bit_num++) {
    for (int sym = 0; sym < nsyms; sym++) {
      if (at(length, sym) == bit_num) {
        int64_t leaf = int64_t(lpos >> 16);
        for (int k = 0; k < bit_num - nbits; k++) {
          if (at(table, leaf) == 0) {
            at(table, int64_t(next_symbol) << 1) = 0;
            at(table, (int64_t(next_symbol) << 1) + 1) = 0;
            at(table, leaf) = uint16_t(next_symbol);
            next_symbol++;
          }
          leaf = int64_t(at(table, leaf)) << 1;
          if (((lpos >> (15 - k)) & 1) == 1) leaf += 1;
        }
        at(table, leaf) = uint16_t(sym);
        lpos += lbit;
        if (lpos > lmask) return false;
      }
    }
    lbit >>= 1;
  }
  if (lpos == lmask) return true;
  for (int sym = 0; sym < nsyms; sym++)
    if (at(length, sym) != 0) return false;
  return true;
}
}  // namespace

// _BitBuffer: bits MSB first, 16-bit little endian words appended below.
struct LzxDecoder::Bits {
  uint64_t buf = 0;
  int bits_left = 0;
  const Bytes& data;
  int64_t pos = 0;
  explicit Bits(const Bytes& d) : data(d) {}
  int64_t size() const { return int64_t(data.size()); }
  uint8_t byte(int64_t i) const { return i >= 0 && i < size() ? data[size_t(i)] : 0; }
  void ensure_bits(int n) {
    while (bits_left < n) {
      uint32_t lo, hi;
      if (pos + 1 < size()) {
        lo = byte(pos);
        hi = byte(pos + 1);
        pos += 2;
      } else if (pos < size()) {
        lo = byte(pos);
        hi = 0;
        pos += 1;
      } else {
        lo = hi = 0;
      }
      buf = (buf << 16) | (hi << 8 | lo);
      bits_left += 16;
    }
  }
  uint32_t peek_bits(int n) const { return uint32_t((buf >> (bits_left - n)) & ((uint64_t(1) << n) - 1)); }
  void remove_bits(int n) {
    bits_left -= n;
    buf &= (uint64_t(1) << bits_left) - 1;
  }
  uint32_t read_bits(int n) {
    if (n == 0) return 0;
    ensure_bits(n);
    uint32_t v = peek_bits(n);
    remove_bits(n);
    return v;
  }
  void reset() {
    buf = 0;
    bits_left = 0;
  }
};

namespace {
// _read_huff_sym
template <class Bits>
int read_huff_sym(std::vector<uint16_t>& table, std::vector<uint8_t>& lengths, int nsyms, int nbits, Bits& bb,
                  int max_codeword) {
  bb.ensure_bits(max_codeword);
  int64_t i = at(table, bb.peek_bits(nbits));
  if (i >= nsyms) {
    int bit_pos = bb.bits_left - nbits - 1;
    while (true) {
      i <<= 1;
      if (bit_pos < 0) return 0;
      if ((bb.buf >> bit_pos) & 1) i |= 1;
      bit_pos--;
      i = at(table, i);
      if (i < nsyms) break;
    }
  }
  bb.remove_bits(at(lengths, i));
  return int(i);
}
}  // namespace

LzxDecoder::LzxDecoder(int window_bits)
    : window_bits_(window_bits),
      window_size_(1u << window_bits),
      num_position_slots_(position_slots(window_bits)),
      main_elements_(kNumChars + (num_position_slots_ << 3)),
      window_(window_size_, 0xDC),
      pretree_table_((1 << kPretreeTablebits) + (kPretreeMaxsymbols << 1)),
      maintree_table_((1 << kMaintreeTablebits) + (kMaintreeMaxsymbols << 1)),
      lentree_table_((1 << kLentreeTablebits) + (kLentreeMaxsymbols << 1)),
      aligntree_table_((1 << kAligntreeTablebits) + (kAligntreeMaxsymbols << 1)),
      pretree_len_(kPretreeMaxsymbols + kLentableSafety),
      maintree_len_(kMaintreeMaxsymbols + kLentableSafety),
      lentree_len_(kLentreeMaxsymbols + kLentableSafety),
      aligntree_len_(kAligntreeMaxsymbols + kLentableSafety) {}

void LzxDecoder::read_lengths(std::vector<uint8_t>& lens, int first, int last, Bits& bb) {
  for (int i = 0; i < kPretreeNum; i++) pretree_len_[size_t(i)] = uint8_t(bb.read_bits(4));
  make_decode_table(kPretreeMaxsymbols, kPretreeTablebits, pretree_len_, pretree_table_);
  int x = first;
  while (x < last) {
    int z = read_huff_sym(pretree_table_, pretree_len_, kPretreeMaxsymbols, kPretreeTablebits, bb, kPretreeMaxCodeword);
    if (z == 17) {
      int y = int(bb.read_bits(4)) + 4;
      for (int k = 0; k < y; k++) at(lens, x++) = 0;
    } else if (z == 18) {
      int y = int(bb.read_bits(5)) + 20;
      for (int k = 0; k < y; k++) at(lens, x++) = 0;
    } else if (z == 19) {
      int y = int(bb.read_bits(1)) + 4;
      z = read_huff_sym(pretree_table_, pretree_len_, kPretreeMaxsymbols, kPretreeTablebits, bb, kPretreeMaxCodeword);
      z = ((at(lens, x) + 17 - z) % 17 + 17) % 17;
      for (int k = 0; k < y; k++) at(lens, x++) = uint8_t(z);
    } else {
      z = ((at(lens, x) + 17 - z) % 17 + 17) % 17;
      at(lens, x++) = uint8_t(z);
    }
  }
}

void LzxDecoder::read_block_header(Bits& bb) {
  if (block_type_ == kUncompressed) {
    if ((block_length_ & 1) == 1) bb.pos += 1;
    bb.reset();
  }
  block_type_ = bb.read_bits(3);
  block_length_ = bb.read_bits(24);
  block_remaining_ = block_length_;
  if (block_type_ == kAligned) {
    for (int i = 0; i < 8; i++) aligntree_len_[size_t(i)] = uint8_t(bb.read_bits(3));
    make_decode_table(kAligntreeMaxsymbols, kAligntreeTablebits, aligntree_len_, aligntree_table_);
  }
  if (block_type_ == kVerbatim || block_type_ == kAligned) {
    read_lengths(maintree_len_, 0, kNumChars, bb);
    read_lengths(maintree_len_, kNumChars, int(main_elements_), bb);
    make_decode_table(kMaintreeMaxsymbols, kMaintreeTablebits, maintree_len_, maintree_table_);
    if (maintree_len_[0xE8] != 0) intel_started_ = true;
    read_lengths(lentree_len_, 0, kSecondaryNumElements, bb);
    make_decode_table(kLentreeMaxsymbols, kLentreeTablebits, lentree_len_, lentree_table_);
  } else if (block_type_ == kUncompressed) {
    intel_started_ = true;
    bb.ensure_bits(16);
    if (bb.bits_left > 16) bb.pos -= 2;
    bb.reset();
    if (bb.pos + 12 <= bb.size()) {
      auto le32 = [&](int64_t p) {
        return uint32_t(bb.byte(p)) | uint32_t(bb.byte(p + 1)) << 8 | uint32_t(bb.byte(p + 2)) << 16 |
               uint32_t(bb.byte(p + 3)) << 24;
      };
      r0_ = le32(bb.pos);
      bb.pos += 4;
      r1_ = le32(bb.pos);
      bb.pos += 4;
      r2_ = le32(bb.pos);
      bb.pos += 4;
    }
  } else {
    throw Error("Invalid LZX block type: " + std::to_string(block_type_));
  }
}

Bytes LzxDecoder::decompress(const Bytes& data, size_t output_size) {
  Bits bb(data);
  Bytes output(output_size);
  size_t out_pos = 0;
  const int64_t frame_size = window_size_;
  const uint32_t window_mask = window_size_ - 1;
  int64_t window_posn = window_pos_, frame_posn = 0;
  if (!header_read_) {
    if (bb.read_bits(1)) {
      uint32_t hi16 = bb.read_bits(16), lo16 = bb.read_bits(16);
      intel_filesize_ = int64_t(hi16) << 16 | lo16;
    }
    intel_started_ = false;
    header_read_ = true;
  }
  while (out_pos < output_size) {
    const int64_t cur_frame_size = std::min<int64_t>(frame_size, int64_t(output_size - out_pos));
    int64_t bytes_todo = frame_posn + cur_frame_size - window_posn;
    if (bytes_todo < 0) bytes_todo = 0;
    while (bytes_todo > 0) {
      if (block_remaining_ == 0) read_block_header(bb);
      int64_t this_run = block_remaining_;
      if (this_run > bytes_todo) this_run = bytes_todo;
      bytes_todo -= this_run;
      block_remaining_ -= this_run;
      if (this_run <= 0) continue;
      if (block_type_ == kUncompressed) {
        for (int64_t k = 0; k < this_run; k++) {
          window_[size_t(window_posn & window_mask)] = bb.byte(bb.pos);
          bb.pos++;
          window_posn++;
        }
      } else {
        while (this_run > 0) {
          int main_element = read_huff_sym(maintree_table_, maintree_len_, kMaintreeMaxsymbols, kMaintreeTablebits, bb,
                                           kMaintreeMaxCodeword);
          if (main_element < kNumChars) {
            window_[size_t(window_posn & window_mask)] = uint8_t(main_element);
            window_posn++;
            this_run--;
            continue;
          }
          main_element -= kNumChars;
          int64_t match_length = main_element & kNumPrimaryLengths;
          if (match_length == kNumPrimaryLengths)
            match_length += read_huff_sym(lentree_table_, lentree_len_, kLentreeMaxsymbols, kLentreeTablebits, bb,
                                          kLentreeMaxCodeword);
          match_length += kMinMatch;
          uint32_t match_offset = uint32_t(main_element >> 3);
          if (match_offset > 2) {
            if (match_offset >= sizeof kExtraBits) throw Error("LZX: a position slot out of range (bad data)");
            const int extra = kExtraBits[match_offset];
            uint32_t verbatim_bits, aligned_bits;
            if (block_type_ == kAligned && extra >= 3) {
              verbatim_bits = bb.read_bits(extra - 3) << 3;
              aligned_bits = uint32_t(read_huff_sym(aligntree_table_, aligntree_len_, kAligntreeMaxsymbols,
                                                    kAligntreeTablebits, bb, kAligntreeMaxCodeword));
            } else {
              verbatim_bits = bb.read_bits(extra);
              aligned_bits = 0;
            }
            match_offset = kPositionBase[match_offset] + verbatim_bits + aligned_bits - 2;
            r2_ = r1_;
            r1_ = r0_;
            r0_ = match_offset;
          } else if (match_offset == 0) {
            match_offset = r0_;
          } else if (match_offset == 1) {
            match_offset = r1_;
            r1_ = r0_;
            r0_ = match_offset;
          } else {
            match_offset = r2_;
            r2_ = r0_;
            r0_ = match_offset;
          }
          this_run -= match_length;
          uint32_t runsrc = uint32_t((window_posn - int64_t(match_offset)) & window_mask);
          for (int64_t k = 0; k < match_length; k++) {
            window_[size_t(window_posn & window_mask)] = window_[runsrc];
            window_posn++;
            runsrc = (runsrc + 1) & window_mask;
          }
        }
        if (this_run < 0) block_remaining_ -= -this_run;
      }
    }
    if (bb.bits_left > 0) bb.ensure_bits(16);
    if (bb.bits_left & 15) bb.remove_bits(bb.bits_left & 15);
    uint32_t wp = uint32_t(frame_posn & window_mask);
    for (int64_t k = 0; k < cur_frame_size; k++) {
      output[out_pos++] = window_[wp];
      wp = (wp + 1) & window_mask;
    }
    frame_posn += cur_frame_size;
  }
  window_pos_ = window_posn & window_mask;
  if (intel_started_ && output_size > 10) e8_decode(output, output_size);
  intel_curpos_ += int64_t(output_size);
  return output;
}

void LzxDecoder::e8_decode(Bytes& data, size_t size) {
  if (intel_curpos_ >= 0x40000000) return;
  size_t i = 0;
  while (i + 10 < size) {
    if (data[i] != 0xE8) {
      i++;
      continue;
    }
    const int64_t curpos = intel_curpos_ + int64_t(i);
    int64_t abs_off = int32_t(uint32_t(data[i + 1]) | uint32_t(data[i + 2]) << 8 | uint32_t(data[i + 3]) << 16 |
                              uint32_t(data[i + 4]) << 24);
    if (abs_off >= -curpos && abs_off < intel_filesize_) {
      const int64_t rel = abs_off >= 0 ? abs_off - curpos : abs_off + intel_filesize_;
      const uint32_t r = uint32_t(rel);
      data[i + 1] = uint8_t(r);
      data[i + 2] = uint8_t(r >> 8);
      data[i + 3] = uint8_t(r >> 16);
      data[i + 4] = uint8_t(r >> 24);
    }
    i += 5;
  }
}

}  // namespace x360ui
