// x360ui-install: the installer (the C++ port of tools/install.py, built up batch by batch).
// This batch has the readers only, as developer commands whose output matches the Python tools':
//   x360ui-install dev stfs PACKAGE [OUT [NAME..]]   as tools/stfs.py
//   x360ui-install dev xex-dump IN.xex OUT.bin       as tools/xex/xex_dump.py
//   x360ui-install dev aes-cbc KEY IV IN OUT         AES-128 CBC decryption (hex KEY and IV), for tests
//   x360ui-install dev lzx WINDOW_BITS SIZE IN OUT   one LZX stream decoded, for tests
#include <cstdio>
#include <fstream>
#include <iostream>
#include <iterator>
#include <set>
#include <string>
#include <vector>

#include "paths.h"
#include "stfs.h"
#include "lzx.h"
#include "xex.h"

using namespace x360ui;

namespace {
Bytes read_file(const std::string& path) {
  std::ifstream f(fs_path(path), std::ios::binary);
  if (!f) throw Error("cannot open " + path);
  return Bytes((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

void write_file(const std::string& path, const Bytes& data) {
  std::ofstream f(fs_path(path), std::ios::binary);
  if (!f) throw Error("cannot write " + path);
  f.write(reinterpret_cast<const char*>(data.data()), std::streamsize(data.size()));
}


void print_line(const std::string& s) {   // the whole string, a NUL too (as Python prints it)
  std::fwrite(s.data(), 1, s.size(), stdout);
  std::fputc('\n', stdout);
}

Bytes hex(const std::string& s) {
  Bytes out;
  for (size_t i = 0; i + 1 < s.size(); i += 2) out.push_back(uint8_t(std::stoi(s.substr(i, 2), nullptr, 16)));
  return out;
}

int dev_stfs(const std::vector<std::string>& a) {
  StfsPackage p(a[0]);
  std::string magic = p.magic;
  while (!magic.empty() && magic.back() == ' ') magic.pop_back();
  std::printf("%s title %08X type %08X, %zu files\n", magic.c_str(), p.title_id, p.content_type, p.files.size());
  if (a.size() == 1) {
    for (const auto& e : p.files) {
      std::printf("%12u ", e.size);
      print_line(e.path);
    }
    return 0;
  }
  std::set<std::string> names;
  for (size_t i = 2; i < a.size(); i++) names.insert(lower_latin1(a[i]));
  for (const auto& e : p.files)
    if (names.empty() || names.count(lower_latin1(e.path))) print_line(p.extract(e, a[1]));
  return 0;
}

int dev_xex(const std::vector<std::string>& a) {
  const XexImage x = xex_dump(read_file(a[0]));
  if (x.image.size() < 2 || x.image[0] != 'M' || x.image[1] != 'Z') {
    std::fprintf(stderr, "decompressed image has no MZ header\n");
    return 1;
  }
  write_file(a[1], x.image);
  std::printf("wrote %zu bytes, base 0x%08X\n", x.image.size(), x.base);
  return 0;
}

int dev_lzx(const std::vector<std::string>& a) {   // WINDOW_BITS SIZE IN OUT: tools/xex/lzx_decompress.py's LZXDecoder
  write_file(a[3], LzxDecoder(std::stoi(a[0])).decompress(read_file(a[2]), size_t(std::stoul(a[1]))));
  return 0;
}

int dev_aes(const std::vector<std::string>& a) {
  const Bytes key = hex(a[0]), iv = hex(a[1]);
  if (key.size() != 16 || iv.size() != 16) throw Error("KEY and IV are 32 hex digits");
  write_file(a[3], aes128_cbc_decrypt(key.data(), iv.data(), read_file(a[2])));
  return 0;
}

int usage() {
  std::fprintf(stderr,
               "x360ui-install dev stfs PACKAGE [OUT [NAME..]]\n"
               "x360ui-install dev xex-dump IN.xex OUT.bin\n"
               "x360ui-install dev aes-cbc KEY IV IN OUT\n"
               "x360ui-install dev lzx WINDOW_BITS SIZE IN OUT\n");
  return 2;
}
}  // namespace

int main(int argc, char** argv) {
  std::vector<std::string> args(argv + 1, argv + argc);
  try {
    if (args.size() >= 3 && args[0] == "dev") {
      const std::string cmd = args[1];
      std::vector<std::string> rest(args.begin() + 2, args.end());
      if (cmd == "stfs") return dev_stfs(rest);
      if (cmd == "xex-dump" && rest.size() == 2) return dev_xex(rest);
      if (cmd == "aes-cbc" && rest.size() == 4) return dev_aes(rest);
      if (cmd == "lzx" && rest.size() == 4) return dev_lzx(rest);
    }
    return usage();
  } catch (const std::exception& e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 1;
  }
}
