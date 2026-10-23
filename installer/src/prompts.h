// The pad prompts drawn with PlayStation's or Nintendo's icons: the C++ port of tools/pad_prompts.py
// (itself a port of the recomp's src/mods/pad_prompts.cpp), the same steps and the same bytes. Its
// image code is its own (no library): the PNG decoder with miniz's inflate standing for zlib's,
// the DDS decoder and encoder in integers, the icon's fitting in doubles as Python computes it.
#pragma once
#include <map>
#include <string>
#include <tuple>
#include <vector>

#include "bytes.h"
#include "pytext.h"

namespace x360ui {

// Python's struct.error (a short read): not a ValueError, so the converter does not catch it.
struct StructError : Error {
  using Error::Error;
};

struct Image {
  int width = 0, height = 0;
  Bytes rgba;
  Image() = default;
  Image(int w, int h) : width(w), height(h), rgba(size_t(w) * size_t(h) * 4, 0) {}
};

extern const char* const kStyles[];          // pad_prompts.STYLES (null-terminated)
bool is_style(const std::string& style);
std::string icon_folder(const std::string& style);
bool swaps_confirm_cancel(const std::string& style, bool setting);

struct PromptIcon {
  std::string image, file;
  double scale;
};
std::vector<PromptIcon> prompt_icons(const std::string& style, bool swap_confirm_cancel);

Image read_png(const Bytes& data);
Image decode_dds(const Bytes& data);
Bytes encode_dds_argb(const Image& img);
Image fit_icon(const Image& icon, int width, int height);
// {image name: (x, y, width, height)}, in the XML's order of first appearance (Python's dict).
std::vector<std::pair<U32, std::tuple<long long, long long, long long, long long>>> image_regions(const U32& xml);

struct LoadedIcon {
  Image image;
  double scale;
};
// patch_texture: (the texture redrawn, how many images were replaced); Error when it fails.
std::pair<Bytes, int> patch_texture(const U32& imageset_xml, const Bytes& texture,
                                    const std::map<U32, LoadedIcon>& icons);
// load_icons from the icons built into the executable (data/prompts, prompts_data.cpp).
std::map<U32, LoadedIcon> load_icons(const std::string& style, bool swap_confirm_cancel);

// The built-in copy of data/prompts (CMake's embed_prompts): "folder/file.png" -> its bytes.
const Bytes* prompt_file(const std::string& name);

}  // namespace x360ui
