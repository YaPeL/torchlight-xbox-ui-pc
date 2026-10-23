// Reads the files out of an Xbox 360 STFS package (CON, LIVE or PIRS): the C++ port of
// tools/stfs.py, the same layout read (see that file's notes) and the same results.
#pragma once
#include <cstdint>
#include <ostream>
#include <fstream>
#include <string>
#include <vector>

#include "bytes.h"

namespace x360ui {

struct StfsEntry {
  std::string name, path;  // path: the folders from the root joined by '/', UTF-8
  bool folder = false, contiguous = false;
  uint32_t blocks = 0, first = 0, size = 0;
  int parent = -1;
};

class StfsPackage {
 public:
  explicit StfsPackage(const std::string& path);

  std::string magic;
  uint32_t header_size = 0, content_type = 0, title_id = 0, total_blocks = 0;
  std::vector<StfsEntry> files;  // the files only, in the file table's order

  uint32_t data_block(uint32_t n) const;
  uint32_t hash_block(uint32_t n) const;
  const StfsEntry* find(const std::string& path) const;
  Bytes read(const StfsEntry& e);
  void read_to(const StfsEntry& e, std::ostream& out);
  std::string extract(const StfsEntry& e, const std::string& dest);  // the file's path

 private:
  std::ifstream f_;
  uint32_t separation_ = 0, table_blocks_ = 0, table_first_ = 0, base_ = 0, shift_ = 0, copy_ = 0;
  Bytes read_block(uint32_t block);
  uint32_t next_block(uint32_t n);
  template <class Visit> void walk(uint32_t first, uint32_t count, bool contiguous, Visit visit);
  void read_table();
};

}  // namespace x360ui
