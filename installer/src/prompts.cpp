#include "prompts.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <cstring>

#include "miniz.h"

namespace x360ui {

const char* const kStyles[] = {"xbox", "playstation3", "playstation4", "playstation5", "playstation", "nintendo", nullptr};

namespace {
std::string style_of(const std::string& s) { return s == "playstation" ? "playstation5" : s; }   // _style

uint32_t rd_be32(const Bytes& d, size_t o) { return be32(d, o); }
uint32_t rd_le32(const Bytes& d, size_t o) {
  need(d.size(), o, 4);
  return uint32_t(d[o]) | uint32_t(d[o + 1]) << 8 | uint32_t(d[o + 2]) << 16 | uint32_t(d[o + 3]) << 24;
}
uint16_t rd_le16(const Bytes& d, size_t o) { return le16(d, o); }

std::string py_bytes_repr(const std::string& b) {   // repr() of a bytes object, for an error's text
  std::string out = "b'";
  for (unsigned char c : b) {
    char buf[8];
    if (c == '\\' || c == '\'') { out += '\\'; out += char(c); }
    else if (c == '\t') out += "\\t";
    else if (c == '\n') out += "\\n";
    else if (c == '\r') out += "\\r";
    else if (c < 0x20 || c >= 0x7F) { std::snprintf(buf, sizeof buf, "\\x%02x", c); out += buf; }
    else out += char(c);
  }
  return out + "'";
}

// Python's round() to an int: half to even (nearbyint in the default rounding mode).
long long py_round(double v) { return (long long)std::nearbyint(v); }
}  // namespace

bool is_style(const std::string& s) {
  for (int i = 0; kStyles[i]; i++)
    if (s == kStyles[i]) return true;
  return false;
}

std::string icon_folder(const std::string& style) {
  return style_of(style).rfind("playstation", 0) == 0 ? "playstation" : "nintendo";
}

bool swaps_confirm_cancel(const std::string& style, bool setting) { return setting && style == "nintendo"; }

std::vector<PromptIcon> prompt_icons(const std::string& style_in, bool swap_confirm_cancel) {
  const std::string style = style_of(style_in);
  std::vector<PromptIcon> icons;
  if (style.rfind("playstation", 0) == 0) {
    const std::string model = std::string("ps") + style.back();
    const char* pairs[][2] = {{"a_button", "cross"}, {"b_button", "circle"}, {"button_x", "square"},
                              {"y_button", "triangle"}, {"left_bumper", "l1"}, {"right_bumper", "r1"},
                              {"left_trigger", "l2"}, {"right_trigger", "r2"}};
    for (auto& p : pairs) icons.push_back({p[0], p[1], 1.0});
    icons.push_back({"start_button", "start_" + model, 1.0});
    icons.push_back({"back_button", "back_" + model, 1.0});
    icons.push_back({"controller", "controller_" + model, 0.8});   // CONTROLLER_SCALE
  } else if (style == "nintendo") {
    const bool swap = swaps_confirm_cancel(style, swap_confirm_cancel);
    icons.push_back({"a_button", swap ? "a" : "b", 1.0});
    icons.push_back({"b_button", swap ? "b" : "a", 1.0});
    const char* pairs[][2] = {{"button_x", "y"}, {"y_button", "x"}, {"left_bumper", "l"}, {"right_bumper", "r"},
                              {"left_trigger", "zl"}, {"right_trigger", "zr"}, {"start_button", "plus"},
                              {"back_button", "minus"}};
    for (auto& p : pairs) icons.push_back({p[0], p[1], 1.0});
    icons.push_back({"controller", "controller", 0.8});
  } else {
    return {};
  }
  const char* sticks[][2] = {{"analog_button_left_lrud", "stick_l"}, {"analog_button_right_lrud", "stick_r"},
                             {"analog_button_left_lr", "stick_l_horizontal"}, {"analog_button_right_lr", "stick_r_horizontal"},
                             {"analog_button_left_ud", "stick_l_vertical"}, {"analog_button_right_ud", "stick_r_vertical"},
                             {"analog_button_left_down", "stick_r_press"}, {"dpad_updown", "dpad_vertical"},
                             {"dpad_leftright", "dpad_horizontal"}, {"dpad_up", "dpad_up"}, {"dpad_down", "dpad_down"},
                             {"dpad_left", "dpad_left"}, {"dpad_right", "dpad_right"}};
  for (auto& p : sticks) icons.push_back({p[0], p[1], 1.0});
  return icons;
}

// ---- PNG (8-bit RGBA or RGB, not interlaced: what the icons are) ----

Image read_png(const Bytes& data) {
  static const uint8_t sig[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
  if (data.size() < 8 || std::memcmp(data.data(), sig, 8) != 0) throw Error("not a PNG");
  size_t pos = 8;
  Bytes idat;
  int width = -1, height = 0, channels = 0;
  while (pos < data.size()) {
    const uint32_t length = rd_be32(data, pos);
    need(data.size(), pos + 4, 4);
    const std::string kind(data.begin() + long(pos + 4), data.begin() + long(pos + 8));
    const Bytes body = slice(data, pos + 8, length);
    if (kind == "IHDR") {
      if (body.size() < 13) throw Error("PNG: a short IHDR");
      width = int(rd_be32(body, 0));
      height = int(rd_be32(body, 4));
      const int depth = body[8], colour = body[9], interlace = body[12];
      if (depth != 8 || (colour != 2 && colour != 6) || interlace) throw Error("only 8-bit RGB or RGBA PNGs, not interlaced");
      channels = colour == 6 ? 4 : 3;
    } else if (kind == "IDAT") {
      idat.insert(idat.end(), body.begin(), body.end());
    }
    pos += 12 + size_t(length);
  }
  if (width < 0) throw Error("no IHDR");
  size_t raw_len = 0;
  void* raw_p = tinfl_decompress_mem_to_heap(idat.data(), idat.size(), &raw_len, TINFL_FLAG_PARSE_ZLIB_HEADER);
  if (!raw_p) throw Error("PNG: its data does not inflate");
  Bytes raw(static_cast<uint8_t*>(raw_p), static_cast<uint8_t*>(raw_p) + raw_len);
  mz_free(raw_p);
  const size_t stride = size_t(width) * size_t(channels);
  Image out(width, height);
  Bytes prev(stride, 0);
  for (int y = 0; y < height; y++) {
    const size_t row = size_t(y) * (stride + 1);
    need(raw.size(), row, stride + 1);
    const int f = raw[row];
    Bytes line(raw.begin() + long(row + 1), raw.begin() + long(row + 1 + stride));
    for (size_t i = 0; i < stride; i++) {
      const int a = i >= size_t(channels) ? line[i - channels] : 0, b = prev[i],
                c = i >= size_t(channels) ? prev[i - channels] : 0;
      if (f == 1) line[i] = uint8_t(line[i] + a);
      else if (f == 2) line[i] = uint8_t(line[i] + b);
      else if (f == 3) line[i] = uint8_t(line[i] + (a + b) / 2);
      else if (f == 4) {
        const int p = a + b - c, pa = std::abs(p - a), pb = std::abs(p - b), pc = std::abs(p - c);
        line[i] = uint8_t(line[i] + (pa <= pb && pa <= pc ? a : pb <= pc ? b : c));
      }
    }
    for (int x = 0; x < width; x++) {
      const size_t o = (size_t(y) * size_t(width) + size_t(x)) * 4, s = size_t(x) * size_t(channels);
      out.rgba[o] = line[s];
      out.rgba[o + 1] = line[s + 1];
      out.rgba[o + 2] = line[s + 2];
      out.rgba[o + 3] = channels == 4 ? line[s + 3] : 255;
    }
    prev = line;
  }
  return out;
}

// ---- DDS ----

namespace {
struct Rgb {
  int r, g, b;
};
Rgb c565(unsigned c) { return {int((c >> 11) & 31) * 255 / 31, int((c >> 5) & 63) * 255 / 63, int(c & 31) * 255 / 31}; }

void colour_block(const Bytes& block, size_t at, bool opaque_only, Rgb pal_out[16], int alpha_out[16]) {
  const unsigned c0 = rd_le16(block, at), c1 = rd_le16(block, at + 2);
  const uint32_t bits = rd_le32(block, at + 4);
  const Rgb p0 = c565(c0), p1 = c565(c1);
  Rgb p2, p3;
  int alpha3;
  if (c0 > c1 || opaque_only) {
    p2 = {(2 * p0.r + p1.r) / 3, (2 * p0.g + p1.g) / 3, (2 * p0.b + p1.b) / 3};
    p3 = {(p0.r + 2 * p1.r) / 3, (p0.g + 2 * p1.g) / 3, (p0.b + 2 * p1.b) / 3};
    alpha3 = 255;
  } else {
    p2 = {(p0.r + p1.r) / 2, (p0.g + p1.g) / 2, (p0.b + p1.b) / 2};
    p3 = {0, 0, 0};
    alpha3 = 0;
  }
  const Rgb pal[4] = {p0, p1, p2, p3};
  for (int i = 0; i < 16; i++) {
    const unsigned k = (bits >> (2 * i)) & 3;
    pal_out[i] = pal[k];
    alpha_out[i] = k == 3 ? alpha3 : 255;
  }
}

int shift_of(uint32_t m) {   // (m & -m).bit_length() - 1, 0 for no mask
  if (!m) return 0;
  int s = 0;
  while (!(m & 1)) { m >>= 1; s++; }
  return s;
}
}  // namespace

Image decode_dds(const Bytes& data) {
  if (data.size() < 4 || std::memcmp(data.data(), "DDS ", 4) != 0) throw Error("not a DDS");
  const int height = int(rd_le32(data, 12)), width = int(rd_le32(data, 16));
  const uint32_t pf_flags = rd_le32(data, 80), bits = rd_le32(data, 88);
  need(data.size(), 84, 4);
  const std::string fourcc(data.begin() + 84, data.begin() + 88);
  Image img(width, height);
  size_t pos = 128;
  if (pf_flags & 0x4) {
    if (fourcc != "DXT1" && fourcc != "DXT3" && fourcc != "DXT5") throw Error("unsupported DDS format " + py_bytes_repr(fourcc));
    const size_t size = fourcc == "DXT1" ? 8 : 16;
    for (int by = 0; by < (height + 3) / 4; by++) {
      for (int bx = 0; bx < (width + 3) / 4; bx++) {
        const Bytes block = slice(data, pos, size);
        pos += size;
        int alphas[16];
        for (int& a : alphas) a = 255;
        if (fourcc == "DXT3") {
          need(block.size(), 0, 8);
          uint64_t a = 0;
          for (int k = 7; k >= 0; k--) a = a << 8 | block[size_t(k)];
          for (int i = 0; i < 16; i++) alphas[i] = int((a >> (4 * i)) & 15) * 17;
        } else if (fourcc == "DXT5") {
          need(block.size(), 0, 8);
          const int a0 = block[0], a1 = block[1];
          uint64_t abits = 0;
          for (int k = 7; k >= 2; k--) abits = abits << 8 | block[size_t(k)];
          int apal[8];
          apal[0] = a0;
          apal[1] = a1;
          if (a0 > a1) {
            for (int i = 0; i < 6; i++) apal[2 + i] = ((6 - i) * a0 + (1 + i) * a1) / 7;
          } else {
            for (int i = 0; i < 4; i++) apal[2 + i] = ((4 - i) * a0 + (1 + i) * a1) / 5;
            apal[6] = 0;
            apal[7] = 255;
          }
          for (int i = 0; i < 16; i++) alphas[i] = apal[(abits >> (3 * i)) & 7];
        }
        Rgb pal[16];
        int alpha1[16];
        colour_block(block, size - 8, fourcc != "DXT1", pal, alpha1);
        for (int i = 0; i < 16; i++) {
          const int x = bx * 4 + i % 4, y = by * 4 + i / 4;
          if (x < width && y < height) {
            const size_t o = (size_t(y) * size_t(width) + size_t(x)) * 4;
            img.rgba[o] = uint8_t(pal[i].r);
            img.rgba[o + 1] = uint8_t(pal[i].g);
            img.rgba[o + 2] = uint8_t(pal[i].b);
            img.rgba[o + 3] = uint8_t(std::min(alpha1[i], alphas[i]));
          }
        }
      }
    }
    return img;
  }
  if (bits == 32) {
    const uint32_t masks[4] = {rd_le32(data, 92), rd_le32(data, 96), rd_le32(data, 100), rd_le32(data, 104)};
    int shifts[4];
    for (int k = 0; k < 4; k++) shifts[k] = shift_of(masks[k]);
    for (size_t i = 0; i < size_t(width) * size_t(height); i++) {
      const uint32_t v = rd_le32(data, pos + 4 * i);
      for (int k = 0; k < 3; k++) img.rgba[4 * i + size_t(k)] = uint8_t((v & masks[k]) >> shifts[k]);
      img.rgba[4 * i + 3] = uint8_t(masks[3] ? (v & masks[3]) >> shifts[3] : 255);
    }
    return img;
  }
  throw Error("unsupported DDS pixel format");
}

Bytes encode_dds_argb(const Image& img) {
  Bytes out(128, 0);
  std::memcpy(out.data(), "DDS ", 4);
  auto put = [&](size_t o, uint32_t v) {
    out[o] = uint8_t(v); out[o + 1] = uint8_t(v >> 8); out[o + 2] = uint8_t(v >> 16); out[o + 3] = uint8_t(v >> 24);
  };
  const uint32_t h7[7] = {124, 0x100F, uint32_t(img.height), uint32_t(img.width), uint32_t(img.width) * 4, 0, 0};
  for (int k = 0; k < 7; k++) put(4 + 4 * size_t(k), h7[k]);
  const uint32_t pf[8] = {32, 0x41, 0, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000};
  for (int k = 0; k < 8; k++) put(76 + 4 * size_t(k), pf[k]);
  put(108, 0x1000);
  out.reserve(128 + img.rgba.size());
  for (size_t i = 0; i < img.rgba.size(); i += 4) {
    out.push_back(img.rgba[i + 2]);
    out.push_back(img.rgba[i + 1]);
    out.push_back(img.rgba[i]);
    out.push_back(img.rgba[i + 3]);
  }
  return out;
}

// ---- fitting ----

Image fit_icon(const Image& icon, int width, int height) {
  Image out(width, height);
  if (!icon.width || !icon.height || !width || !height) return out;
  const double scale = std::min(double(width) / icon.width, double(height) / icon.height);
  const int w = int(std::min<long long>(std::max<long long>(py_round(icon.width * scale), 1), width));
  const int h = int(std::min<long long>(std::max<long long>(py_round(icon.height * scale), 1), height));
  const int left = (width - w) / 2, top = (height - h) / 2;
  const double sx = double(icon.width) / w, sy = double(icon.height) / h;
  for (int y = 0; y < h; y++) {
    const double y0 = y * sy, y1 = (y + 1) * sy;
    for (int x = 0; x < w; x++) {
      const double x0 = x * sx, x1 = (x + 1) * sx;
      double s[4] = {0.0, 0.0, 0.0, 0.0};
      double area = 0.0;
      long long iy = (long long)y0;
      while (iy < icon.height && iy < y1) {
        const double wy = std::min(double(iy + 1), y1) - std::max(double(iy), y0);
        long long ix = (long long)x0;
        while (ix < icon.width && ix < x1) {
          const double wgt = (std::min(double(ix + 1), x1) - std::max(double(ix), x0)) * wy;
          if (wgt > 0) {
            const size_t o = (size_t(iy) * size_t(icon.width) + size_t(ix)) * 4;
            const double a = icon.rgba[o + 3] / 255.0;
            s[0] += icon.rgba[o] * a * wgt;
            s[1] += icon.rgba[o + 1] * a * wgt;
            s[2] += icon.rgba[o + 2] * a * wgt;
            s[3] += icon.rgba[o + 3] * wgt;
            area += wgt;
          }
          ix++;
        }
        iy++;
      }
      if (area <= 0 || s[3] <= 0) continue;
      const double alpha = s[3] / area;
      const size_t q = (size_t(top + y) * size_t(width) + size_t(left + x)) * 4;
      for (int c = 0; c < 3; c++)
        out.rgba[q + size_t(c)] = uint8_t(std::min<long long>(std::max<long long>(py_round(s[c] / area * 255.0 / alpha), 0), 255));
      out.rgba[q + 3] = uint8_t(std::min<long long>(std::max<long long>(py_round(alpha), 0), 255));
    }
  }
  return out;
}

std::vector<std::pair<U32, std::tuple<long long, long long, long long, long long>>> image_regions(const U32& xml) {
  std::vector<std::pair<U32, std::tuple<long long, long long, long long, long long>>> regions;
  for (const Match& m : find_tags(xml, U"Image")) {
    const auto attrs = word_attrs(substr(xml, m.all));
    std::optional<long long> v[4];
    const char32_t* keys[4] = {U"XPos", U"YPos", U"Width", U"Height"};
    auto name = attrs.find(U"Name");
    bool ok = name != attrs.end();
    for (int k = 0; ok && k < 4; k++) {
      auto it = attrs.find(keys[k]);
      if (it == attrs.end() || !(v[k] = py_int(it->second))) ok = false;
    }
    if (!ok) continue;
    const auto value = std::make_tuple(*v[0], *v[1], *v[2], *v[3]);
    auto at = std::find_if(regions.begin(), regions.end(), [&](const auto& r) { return r.first == name->second; });
    if (at != regions.end()) at->second = value;
    else regions.push_back({name->second, value});
  }
  return regions;
}

std::pair<Bytes, int> patch_texture(const U32& imageset_xml, const Bytes& texture, const std::map<U32, LoadedIcon>& icons) {
  Image img = decode_dds(texture);
  int count = 0;
  for (const auto& [name, r] : image_regions(imageset_xml)) {
    auto it = icons.find(name);
    if (it == icons.end()) continue;
    const auto [x, y, w, h] = r;
    const double scale = it->second.scale;
    if (x < 0 || y < 0 || w <= 0 || h <= 0 || x + w > img.width || y + h > img.height)
      throw Error("image " + to_utf8(name) + " lies outside its texture");
    const long long margin = w > 2 && h > 2 ? 1 : 0;   // PROMPT_MARGIN
    const Image fitted = fit_icon(it->second.image, int(std::max<long long>(1, py_round(double(w - 2 * margin) * scale))),
                                  int(std::max<long long>(1, py_round(double(h - 2 * margin) * scale))));
    const long long left = x + (w - fitted.width) / 2, top = y + (h - fitted.height) / 2;
    for (long long row = 0; row < h; row++) {
      const size_t o = (size_t(y + row) * size_t(img.width) + size_t(x)) * 4;
      std::fill(img.rgba.begin() + long(o), img.rgba.begin() + long(o + size_t(w) * 4), uint8_t(0));
    }
    for (int row = 0; row < fitted.height; row++) {
      const size_t o = (size_t(top + row) * size_t(img.width) + size_t(left)) * 4;
      std::copy(fitted.rgba.begin() + long(size_t(row) * size_t(fitted.width) * 4),
                fitted.rgba.begin() + long(size_t(row + 1) * size_t(fitted.width) * 4), img.rgba.begin() + long(o));
    }
    count++;
  }
  return {encode_dds_argb(img), count};
}

std::map<U32, LoadedIcon> load_icons(const std::string& style, bool swap_confirm_cancel) {
  std::map<U32, LoadedIcon> icons;
  for (const PromptIcon& p : prompt_icons(style, swap_confirm_cancel)) {
    const std::string name = icon_folder(style) + "/" + p.file + ".png";
    const Bytes* data = prompt_file(name);
    if (!data) throw Error("no built-in icon " + name);
    icons[from_ascii(p.image.c_str())] = {read_png(*data), p.scale};
  }
  return icons;
}

}  // namespace x360ui
