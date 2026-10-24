// UTF-8 strings to and from std::filesystem paths (wide on Windows).
#pragma once
#include <filesystem>
#include <string>

namespace x360ui {

inline std::filesystem::path fs_path(const std::string& utf8) {
#if defined(_WIN32)
  return std::filesystem::u8path(utf8);
#else
  return std::filesystem::path(utf8);
#endif
}

inline std::string utf8_path(const std::filesystem::path& p) {
#if defined(_WIN32)
  auto s = p.u8string();
  return std::string(s.begin(), s.end());
#else
  return p.string();
#endif
}

}  // namespace x360ui
