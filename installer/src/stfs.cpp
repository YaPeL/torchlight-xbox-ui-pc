#include "stfs.h"

#include <algorithm>
#include <filesystem>
#include <map>
#include <sstream>

#include "paths.h"

namespace x360ui {
namespace {
constexpr uint32_t kBlock = 0x1000, kPerTable = 0xAA;

std::string latin1_to_utf8(const uint8_t* p, size_t n) {
  std::string out;
  for (size_t i = 0; i < n; i++) {
    if (p[i] < 0x80) {
      out += char(p[i]);
    } else {
      out += char(0xC0 | p[i] >> 6);
      out += char(0x80 | (p[i] & 0x3F));
    }
  }
  return out;
}

// Python's str.lower() on a name read as Latin-1 (and kept as UTF-8): ASCII and Latin-1's
// capitals (U+00C0-U+00DE but U+00D7).
std::string lower_ascii(std::string s) {
  for (size_t i = 0; i < s.size(); i++) {
    unsigned char c = (unsigned char)s[i];
    if (c >= 'A' && c <= 'Z') s[i] = char(c - 'A' + 'a');
    else if (c == 0xC3 && i + 1 < s.size()) {               // U+00C0-U+00FF: C3 80-BF
      unsigned char d = (unsigned char)s[i + 1];
      if (d >= 0x80 && d <= 0x9E && d != 0x97) s[i + 1] = char(d + 0x20);
      i++;
    }
  }
  return s;
}
}  // namespace

StfsPackage::StfsPackage(const std::string& path) : f_(fs_path(path), std::ios::binary) {
  if (!f_) throw Error("cannot open " + path);
  Bytes head(0x3A0);
  f_.read(reinterpret_cast<char*>(head.data()), head.size());
  head.resize(size_t(f_.gcount()));
  f_.clear();
  magic.assign(head.begin(), head.begin() + std::min<size_t>(4, head.size()));
  if (magic != "CON " && magic != "LIVE" && magic != "PIRS")
    throw Error("not an Xbox 360 package (CON, LIVE or PIRS)");
  header_size = be32(head, 0x340);
  content_type = be32(head, 0x344);
  title_id = be32(head, 0x360);
  const size_t v = 0x379;
  need(head.size(), v + 2, 6);
  separation_ = head[v + 2];
  table_blocks_ = le16(head, v + 3);
  table_first_ = le24(head, v + 5);
  base_ = (header_size + 0xFFF) & ~0xFFFu;
  shift_ = base_ == 0xB000 || (separation_ & 1) ? 0 : 1;
  total_blocks = head.size() >= v + 0x20 ? be32(head, v + 0x1c) : 0;
  // Two copies of each table (shift 1): the active one of a package of one level 0 table is told
  // by the separation's bit 1; deeper packages' choice is not read, the first copy taken.
  copy_ = shift_ && (separation_ & 2) && total_blocks <= kPerTable ? 1 : 0;
  read_table();
}

uint32_t StfsPackage::data_block(uint32_t n) const {
  const uint32_t s = shift_;
  uint32_t b = n + (((n + kPerTable) / kPerTable) << s);
  if (n < kPerTable) return b;
  b += ((n + kPerTable * kPerTable) / (kPerTable * kPerTable)) << s;
  if (n < kPerTable * kPerTable) return b;
  return b + (1u << s);
}

uint32_t StfsPackage::hash_block(uint32_t n) const {
  const uint32_t s = shift_, step = kPerTable + (1u << s);
  if (n < kPerTable) return 0;
  uint32_t b = (n / kPerTable) * step;
  b += ((n / (kPerTable * kPerTable)) + 1) << s;
  if (n < kPerTable * kPerTable) return b;
  return b + (1u << s);
}

Bytes StfsPackage::read_block(uint32_t block) {
  Bytes out(kBlock);
  f_.clear();
  f_.seekg(std::streamoff(base_) + std::streamoff(block) * kBlock);
  f_.read(reinterpret_cast<char*>(out.data()), kBlock);
  out.resize(size_t(f_.gcount()));
  f_.clear();
  return out;
}

uint32_t StfsPackage::next_block(uint32_t n) {
  f_.clear();
  f_.seekg(std::streamoff(base_) + std::streamoff(hash_block(n) + copy_) * kBlock + (n % kPerTable) * 0x18 + 0x15);
  uint8_t got[3];
  f_.read(reinterpret_cast<char*>(got), 3);
  const auto count = f_.gcount();
  f_.clear();
  if (count < 3) throw Error("the package ends inside its hash tables");
  return uint32_t(got[0]) << 16 | uint32_t(got[1]) << 8 | got[2];
}

// As stfs.py's blocks() generator consumed by a for loop: the next block is computed each time the
// loop asks for one more, the last block's next included when the loop runs to the end. visit
// returns false to stop (the loop's break).
template <class Visit>
void StfsPackage::walk(uint32_t first, uint32_t count, bool contiguous, Visit visit) {
  uint32_t n = first;
  for (uint32_t i = 0; i < count; i++) {
    if (i) n = contiguous ? n + 1 : next_block(n);
    if (!visit(n)) return;
  }
  if (count && !contiguous) next_block(n);
}

void StfsPackage::read_table() {
  std::map<int, StfsEntry> entries;  // by the entry's index in the whole table
  uint32_t t = 0;
  walk(table_first_, table_blocks_, false, [&](uint32_t n) {
    const Bytes data = read_block(data_block(n));
    for (uint32_t o = 0; o < kBlock; o += 0x40) {
      if (o + 0x40 > data.size()) continue;
      const uint8_t* e = data.data() + o;
      const unsigned length = e[0x28] & 0x3F;
      if (!length) continue;
      StfsEntry x;
      x.name = latin1_to_utf8(e, length);
      x.folder = e[0x28] & 0x80;
      x.contiguous = e[0x28] & 0x40;
      x.blocks = le24(data, o + 0x29);
      x.first = le24(data, o + 0x2F);
      x.parent = int16_t(be16(data, o + 0x32));
      x.size = be32(data, o + 0x34);
      entries[int(t * (kBlock / 0x40) + o / 0x40)] = x;
    }
    t++;
    return true;
  });
  for (auto& [index, e] : entries) {
    std::vector<std::string> parts{e.name};
    int p = e.parent, seen = 0;
    while (entries.count(p) && seen < 64) {
      parts.push_back(entries[p].name);
      p = entries[p].parent;
      seen++;
    }
    std::string path;
    for (auto it = parts.rbegin(); it != parts.rend(); ++it) path += (path.empty() ? "" : "/") + *it;
    e.path = path;
  }
  for (auto& [index, e] : entries)
    if (!e.folder) files.push_back(e);
}

const StfsEntry* StfsPackage::find(const std::string& path) const {
  std::string low = path;
  std::replace(low.begin(), low.end(), '\\', '/');
  low = lower_ascii(low);
  for (const auto& e : files)
    if (lower_ascii(e.path) == low) return &e;
  return nullptr;
}

void StfsPackage::read_to(const StfsEntry& e, std::ostream& out) {
  int64_t left = e.size;
  walk(e.first, e.blocks, e.contiguous, [&](uint32_t n) {
    if (left <= 0) return false;
    Bytes data = read_block(data_block(n));
    if (int64_t(data.size()) > left) data.resize(size_t(left));
    left -= int64_t(data.size());
    out.write(reinterpret_cast<const char*>(data.data()), std::streamsize(data.size()));
    return true;
  });
  if (left > 0) throw Error(e.path + ": the package ends before the file");
}

Bytes StfsPackage::read(const StfsEntry& e) {
  std::ostringstream out;
  read_to(e, out);
  const std::string s = out.str();
  return Bytes(s.begin(), s.end());
}

std::string StfsPackage::extract(const StfsEntry& e, const std::string& dest) {
  std::vector<std::string> parts;
  std::stringstream ss(e.path);
  for (std::string p; std::getline(ss, p, '/');) parts.push_back(p);
  if (e.path.empty() || e.path.back() == '/') parts.push_back("");
  for (const auto& p : parts)
    if (p.empty() || p == "." || p == ".." || p.find('\\') != std::string::npos || p.find(':') != std::string::npos ||
        p.find('\0') != std::string::npos)   // Python refuses an embedded NUL; a path here would end at it
      throw Error("a name that would leave the folder: '" + e.path + "'");
  std::filesystem::path path = fs_path(dest);
  for (const auto& p : parts) path /= fs_path(p);
  std::filesystem::create_directories(path.parent_path());
  std::ofstream out(path, std::ios::binary);
  if (!out) throw Error("cannot write " + utf8_path(path));
  read_to(e, out);
  return utf8_path(path);
}

}  // namespace x360ui
