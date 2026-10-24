// x360ui-install: the installer (the C++ port of tools/install.py, built up batch by batch).
// This batch has the readers only, as developer commands whose output matches the Python tools':
//   x360ui-install dev stfs PACKAGE [OUT [NAME..]]   as tools/stfs.py
//   x360ui-install dev xex-dump IN.xex OUT.bin       as tools/xex/xex_dump.py
//   x360ui-install dev aes-cbc KEY IV IN OUT         AES-128 CBC decryption (hex KEY and IV), for tests
//   x360ui-install dev lzx WINDOW_BITS SIZE IN OUT   one LZX stream decoded, for tests
//   x360ui-install dev convert PAK OUT [--prompts STYLE] [--swap-confirm-cancel] [--image IMAGE] [--pc-pak PCPAK]
//                                                    as tools/convert_x360_ui.py
#include <cstdio>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif
#include <fstream>
#include <iostream>
#include <iterator>
#include <set>
#include <string>
#include <vector>

#include "paths.h"
#include "stfs.h"
#include "convert.h"
#include "install.h"
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

int usage();

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

int dev_convert(std::vector<std::string> a) {
  ConvertOptions o;
  for (size_t i = 0; i < a.size();) {
    if (a[i] == "--swap-confirm-cancel") { o.swap_confirm_cancel = true; a.erase(a.begin() + long(i)); continue; }
    if ((a[i] == "--prompts" || a[i] == "--image" || a[i] == "--pc-pak") && i + 1 < a.size()) {
      (a[i] == "--prompts" ? o.style : a[i] == "--image" ? o.image : o.pc_pak) = a[i + 1];
      a.erase(a.begin() + long(i), a.begin() + long(i) + 2);
      continue;
    }
    i++;
  }
  if (a.size() != 2) return usage();
  o.pak = a[0];
  o.out = a[1];
  for (const std::string& line : convert_ui(o)) std::printf("%s\n", line.c_str());
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
               "x360ui-install dev lzx WINDOW_BITS SIZE IN OUT\n"
               "x360ui-install dev convert PAK OUT [--prompts STYLE] [--swap-confirm-cancel] [--image IMAGE] [--pc-pak PCPAK]\n");
  return 2;
}
}  // namespace

#ifdef _WIN32
// A window program has no console. Its output goes where it was redirected (a pipe, a file); else,
// run from a terminal, to that terminal's console; on a double click, nowhere (the windows say it).
static void use_parent_console() {
  const auto unset = [](DWORD which) {
    const HANDLE h = GetStdHandle(which);
    return h == nullptr || h == INVALID_HANDLE_VALUE;
  };
  const bool out = unset(STD_OUTPUT_HANDLE), err = unset(STD_ERROR_HANDLE), in = unset(STD_INPUT_HANDLE);
  if (!(out || err || in) || !AttachConsole(ATTACH_PARENT_PROCESS)) return;
  if (in) set_shared_console(true);
  FILE* f = nullptr;
  if (out) freopen_s(&f, "CONOUT$", "w", stdout);
  if (err) freopen_s(&f, "CONOUT$", "w", stderr);
  if (in) freopen_s(&f, "CONIN$", "r", stdin);
  std::ios::sync_with_stdio(true);
  std::cout.clear(); std::cerr.clear(); std::cin.clear();
}
#endif

int main(int argc, char** argv) {
#ifdef _WIN32
  use_parent_console();
#endif
  std::vector<std::string> args(argv + 1, argv + argc);
  try {
    if (args.size() >= 2 && args[0] == "dev") {
      const std::string cmd = args[1];
      std::vector<std::string> rest(args.begin() + 2, args.end());
      if (cmd == "stfs" && !rest.empty()) return dev_stfs(rest);
      if (cmd == "xex-dump" && rest.size() == 2) return dev_xex(rest);
      if (cmd == "aes-cbc" && rest.size() == 4) return dev_aes(rest);
      if (cmd == "lzx" && rest.size() == 4) return dev_lzx(rest);
      if (cmd == "convert") return dev_convert(rest);
      if (cmd == "strings-table") {
        for (const auto& [k, a] : strings_table()) std::printf("%s=0x%08X\n", k.c_str(), a);
        return 0;
      }
    }
    if (!args.empty() && args[0] == "dev") return usage();
    return installer_main(args);
  } catch (const std::exception& e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 1;
  }
}
