// The 360 interface's files converted for the PC game: the C++ port of tools/convert_x360_ui.py,
// whose output it gives byte for byte (the files, x360_ui_report.json, x360_strings.txt).
#pragma once
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "bytes.h"
#include "pytext.h"

namespace x360ui {

// An insertion-ordered map of path -> bytes (Python's dict of the converted files).
struct Files {
  std::vector<std::pair<std::string, Bytes>> items;
  Bytes* find(const std::string& path);
  const Bytes* find(const std::string& path) const;
  void set(const std::string& path, Bytes data);   // replaces in place, or appends
};

struct ConvertOptions {
  std::string pak, out, style = "xbox", image, pc_pak;
  bool swap_confirm_cancel = false;
};

// convert_x360_ui.main: writes out/media/ui/x360/..., the shared files, x360_ui_report.json and
// (with an image or a PC pak) x360_strings.txt; returns the lines Python prints.
std::vector<std::string> convert_ui(const ConvertOptions& opt);

// For tests: the 360 code's texts' keys and addresses (STRINGS, then STRINGS_UTF8 as "utf8:KEY").
std::vector<std::pair<std::string, uint32_t>> strings_table();
// For tests.
U32 label_button_relative(const U32& text);
U32 label_frame_relative(const U32& text);

}  // namespace x360ui
