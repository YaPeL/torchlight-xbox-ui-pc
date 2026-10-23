// LZX decompression as used by Xbox 360 XEX2 files: the C++ port of tools/xex/lzx_decompress.py
// (from https://github.com/sp00nznet/360tools, MIT License, Copyright (c) 2026 sp00nznet), derived
// from libmspack's LZX decoder ((C) 2003-2023 Stuart Caie, originally LGPL-2.1; under GPL-3.0 per
// its section 3; the notices in installer/third_party/NOTICES.md), the same algorithm step for step.
#pragma once
#include <cstdint>
#include <vector>

#include "bytes.h"

namespace x360ui {

class LzxDecoder {
 public:
  explicit LzxDecoder(int window_bits);
  Bytes decompress(const Bytes& data, size_t output_size);

 private:
  struct Bits;
  void read_lengths(std::vector<uint8_t>& lens, int first, int last, Bits& bb);
  void read_block_header(Bits& bb);
  void e8_decode(Bytes& data, size_t size);

  int window_bits_;
  uint32_t window_size_, num_position_slots_, main_elements_;
  std::vector<uint8_t> window_;
  int64_t window_pos_ = 0;
  uint32_t r0_ = 1, r1_ = 1, r2_ = 1;
  std::vector<uint16_t> pretree_table_, maintree_table_, lentree_table_, aligntree_table_;
  std::vector<uint8_t> pretree_len_, maintree_len_, lentree_len_, aligntree_len_;
  int64_t block_remaining_ = 0;
  uint32_t block_length_ = 0, block_type_ = 0;
  bool header_read_ = false, intel_started_ = false;
  int64_t intel_filesize_ = 0, intel_curpos_ = 0;
};

}  // namespace x360ui
