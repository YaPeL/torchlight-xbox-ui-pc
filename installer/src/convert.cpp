// The port of tools/convert_x360_ui.py: the same steps in the same order, so that the files, the
// report and the strings come out byte for byte as Python writes them.
#include "convert.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <set>
#include <variant>

#include "miniz.h"
#include "paths.h"
#include "prompts.h"

namespace x360ui {

Bytes* Files::find(const std::string& path) {
  for (auto& [p, d] : items)
    if (p == path) return &d;
  return nullptr;
}
const Bytes* Files::find(const std::string& path) const {
  for (const auto& [p, d] : items)
    if (p == path) return &d;
  return nullptr;
}
void Files::set(const std::string& path, Bytes data) {
  if (Bytes* d = find(path)) *d = std::move(data);
  else items.push_back({path, std::move(data)});
}

namespace {

const char* const kScheme = "media/ui/hud/schemes/ceguiwidgets.scheme";
const char* const kDest = "media/ui/x360/";
const U32 kPrefix = U"X360_";
const char* const kLabelLook = "ceguiWidget/ItemTextButton";
constexpr double kLabelHeight720 = 17.27;
constexpr uint32_t kImageBase = 0x82000000;

// ---- JSON, as json.dump(obj, f, indent=1) writes it (ensure_ascii, insertion order) ----
struct Json;
using JsonList = std::vector<Json>;
using JsonDict = std::vector<std::pair<std::string, Json>>;
struct Json {
  std::variant<std::string, long long, std::shared_ptr<JsonList>, std::shared_ptr<JsonDict>> v;
  static Json str(std::string s) { return Json{std::move(s)}; }
  static Json num(long long n) { return Json{n}; }
  static Json list() { return Json{std::make_shared<JsonList>()}; }
  static Json dict() { return Json{std::make_shared<JsonDict>()}; }
  JsonList& l() { return *std::get<std::shared_ptr<JsonList>>(v); }
  JsonDict& d() { return *std::get<std::shared_ptr<JsonDict>>(v); }
  Json& operator[](const std::string& key) {   // Python's d[key]: the place kept on a reassignment
    for (auto& [k, j] : d())
      if (k == key) return j;
    d().push_back({key, Json::str("")});
    return d().back().second;
  }
};

void json_string(std::string& out, const std::string& utf8) {
  out += '"';
  for (char32_t c : from_utf8(utf8)) {
    char buf[16];
    if (c == '"') out += "\\\"";
    else if (c == '\\') out += "\\\\";
    else if (c == '\n') out += "\\n";
    else if (c == '\r') out += "\\r";
    else if (c == '\t') out += "\\t";
    else if (c == '\b') out += "\\b";
    else if (c == '\f') out += "\\f";
    else if (c < 0x20 || c >= 0x7F) {
      if (c >= 0x10000) {
        const char32_t v = c - 0x10000;
        std::snprintf(buf, sizeof buf, "\\u%04x\\u%04x", unsigned(0xD800 + (v >> 10)), unsigned(0xDC00 + (v & 0x3FF)));
      } else {
        std::snprintf(buf, sizeof buf, "\\u%04x", unsigned(c));
      }
      out += buf;
    } else {
      out += char(c);
    }
  }
  out += '"';
}

void json_write(std::string& out, const Json& j, int depth) {
  const std::string pad(size_t(depth + 1), ' '), close(size_t(depth), ' ');
  if (auto s = std::get_if<std::string>(&j.v)) return json_string(out, *s);
  if (auto n = std::get_if<long long>(&j.v)) { out += std::to_string(*n); return; }
  if (auto l = std::get_if<std::shared_ptr<JsonList>>(&j.v)) {
    if ((*l)->empty()) { out += "[]"; return; }
    out += "[\n";
    for (size_t i = 0; i < (*l)->size(); i++) {
      out += pad;
      json_write(out, (**l)[i], depth + 1);
      out += i + 1 < (*l)->size() ? ",\n" : "\n";
    }
    out += close + "]";
    return;
  }
  const JsonDict& d = *std::get<std::shared_ptr<JsonDict>>(j.v);
  if (d.empty()) { out += "{}"; return; }
  out += "{\n";
  for (size_t i = 0; i < d.size(); i++) {
    out += pad;
    json_string(out, d[i].first);
    out += ": ";
    json_write(out, d[i].second, depth + 1);
    out += i + 1 < d.size() ? ",\n" : "\n";
  }
  out += close + "}";
}

// Python's text mode: "\n" written as the system's line end.
Bytes text_mode(const std::string& s) {
#ifdef _WIN32
  std::string t;
  for (char c : s) {
    if (c == '\n') t += '\r';
    t += c;
  }
  return Bytes(t.begin(), t.end());
#else
  return Bytes(s.begin(), s.end());
#endif
}

std::string u8(const U32& s) { return to_utf8(s); }
U32 u32(const std::string& s) { return from_utf8(s); }

std::string basename(std::string p) {
  std::replace(p.begin(), p.end(), '\\', '/');
  const size_t k = p.rfind('/');
  return k == std::string::npos ? p : p.substr(k + 1);
}

std::string lower_path(const std::string& s) { return u8(py_lower_ascii(u32(s))); }

// An entry's name lowercased for a filter: ASCII lowered; a name outside ASCII stops only when the
// answer could depend on it (it passes the filter, or holds one of the two characters whose
// lowercase is ASCII: U+0130, U+212A).
std::string lower_for_filter(const std::string& s, const std::function<bool(const std::string&)>& passes) {
  std::string low = s;
  bool ascii = true;
  for (char& c : low) {
    if ((unsigned char)c >= 0x80) ascii = false;
    else if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
  }
  if (ascii) return low;
  if (passes(low) || s.find("\xC4\xB0") != std::string::npos || s.find("\xE2\x84\xAA") != std::string::npos)
    throw NonAscii("a zip entry's name outside ASCII that a filter takes");
  return low;
}

bool ends_with(const std::string& s, const std::string& e) { return s.size() >= e.size() && s.compare(s.size() - e.size(), e.size(), e) == 0; }


// ---- reading the paks ----
struct Zip {
  mz_zip_archive z{};
  std::string path_;
  // zipfile (Python 3.12 on) takes an entry's name from its Info-ZIP Unicode Path field (0x7075)
  // when that field's version is 1 and its CRC is the raw name's: the UTF-8 name it holds, cut at
  // a NUL. Returns true with that name, false when there is none to use; Error as Python's
  // BadZipFile for a corrupt field.
  // Bytes of the archive at an offset from its start (miniz's offsets: data before the zip kept out).
  bool at(uint64_t ofs, void* buf, size_t n) { return mz_zip_read_archive_data(&z, ofs, buf, n) == n; }
  bool unicode_path(uint64_t ofs, const std::string& raw, std::string& out) {
    unsigned char h[46];
    if (!at(ofs, h, 46)) return false;
    const unsigned name = h[28] | h[29] << 8, extra = h[30] | h[31] << 8;
    std::vector<unsigned char> x(extra);
    if (extra && !at(ofs + 46 + name, x.data(), extra)) return false;
    bool got = false;
    for (size_t k = 0; k + 4 <= x.size();) {
      const unsigned id = x[k] | x[k + 1] << 8, len = x[k + 2] | x[k + 3] << 8;
      if (k + 4 + len > x.size()) throw Error("Corrupt extra field");   // zipfile's BadZipFile, for any field
      if (id == 0x7075) {
        const size_t end = k + 4 + len;
        if (end - (k + 4) < 5) throw Error("Corrupt unicode path extra field (0x7075)");
        const unsigned version = x[k + 4];
        const uint32_t crc = uint32_t(x[k + 5]) | uint32_t(x[k + 6]) << 8 | uint32_t(x[k + 7]) << 16 | uint32_t(x[k + 8]) << 24;
        const uint32_t name_crc = uint32_t(mz_crc32(MZ_CRC32_INIT, reinterpret_cast<const unsigned char*>(raw.data()), raw.size()));
        if (version == 1 && crc == name_crc) {
          std::string up(x.begin() + long(k + 9), x.begin() + long(end));
          from_utf8(up);   // invalid UTF-8: BadZipFile
          if (!up.empty()) { out = up; got = true; }
        }
      }
      k += 4 + len;
    }
    return got;
  }
  std::vector<std::string> names_;
  explicit Zip(const std::string& path) : path_(path) {
    if (!mz_zip_reader_init_file(&z, path.c_str(), 0)) throw Error("cannot read " + path + " as a zip");
    // zipfile reads every entry's name when it opens the zip: its refusals come here, for all.
    try {
      for (unsigned i = 0; i < count(); i++) name(i);
    } catch (...) {
      mz_zip_reader_end(&z);
      throw;
    }
  }
  ~Zip() { mz_zip_reader_end(&z); }
  unsigned count() { return mz_zip_reader_get_num_files(&z); }
  // The entry's name as zipfile gives it: up to a NUL; on Windows its backslashes made slashes.
  // Python reads a name without the UTF-8 flag as cp437: one with bytes above 127 then stops here.
  std::string name(unsigned i) {
    if (i < names_.size() && !names_[i].empty()) return names_[i];
    mz_zip_archive_file_stat st0;
    std::string raw;   // the name's bytes in full (the Unicode Path field's CRC is of them)
    if (mz_zip_reader_file_stat(&z, i, &st0)) {
      const uint64_t rec = z.m_central_directory_file_ofs + st0.m_central_dir_ofs;   // from the archive's start
      unsigned char h[46];
      if (at(rec, h, 46)) {
        raw.resize(size_t(h[28] | h[29] << 8));
        if (!raw.empty()) at(rec + 46, &raw[0], raw.size());
      }
    }
    std::string buf = raw.c_str();   // zipfile cuts a name at its first NUL
#ifdef _WIN32
    std::replace(buf.begin(), buf.end(), '\\', '/');
#endif
    mz_zip_archive_file_stat st;
    if (mz_zip_reader_file_stat(&z, i, &st)) {
      // Python decodes the name first (strict UTF-8 with the flag, else cp437), then applies the field.
      if (st.m_bit_flag & 0x800) from_utf8(raw.c_str());
      std::string up;
      if (unicode_path(z.m_central_directory_file_ofs + st.m_central_dir_ofs, raw, up)) {
        buf = up.c_str();
      } else if (!(st.m_bit_flag & 0x800)) {
        for (unsigned char c : buf)
          if (c >= 0x80) throw NonAscii("a zip entry's name in cp437 (no UTF-8 flag) outside ASCII");
      }
#ifdef _WIN32
      std::replace(buf.begin(), buf.end(), '\\', '/');
#endif
    }
    if (i >= names_.size()) names_.resize(i + 1);
    names_[i] = buf;
    return buf;
  }
  Bytes read(unsigned i) {
    size_t n = 0;
    void* p = mz_zip_reader_extract_to_heap(&z, i, &n, 0);
    if (!p) throw Error("cannot inflate " + name(i));
    Bytes out(static_cast<uint8_t*>(p), static_cast<uint8_t*>(p) + n);
    mz_free(p);
    return out;
  }
  // The last entry of that name, as zipfile's read(name) takes it; -1 for none.
  std::map<std::string, long> last_;
  long index_of(const std::string& n) {
    if (last_.empty())
      for (unsigned i = 0; i < count(); i++) last_[name(i)] = long(i);
    auto it = last_.find(n);
    return it == last_.end() ? -1 : it->second;
  }
};

const std::set<std::string> kExtraLayouts = {"interactivemenu.layout"};

std::vector<std::pair<std::string, Bytes>> read_pak(const std::string& path) {
  static const char* const wanted[] = {".uilayout", ".scheme", ".looknfeel", ".imageset", ".font", ".dds", ".ttf", ".tga", ".png"};
  Zip z(path);
  std::vector<std::pair<std::string, Bytes>> files;   // dict: a repeated key keeps its place
  auto wanted_name = [&](const std::string& low) {
    if (low.rfind("media/ui/", 0) != 0) return false;
    bool ok = kExtraLayouts.count(basename(low)) > 0;
    for (const char* w : wanted) ok = ok || ends_with(low, w);
    return ok;
  };
  for (unsigned i = 0; i < z.count(); i++) {
    const std::string low = lower_for_filter(z.name(i), wanted_name);
    if (!wanted_name(low)) continue;
    Bytes data = z.read(i);
    auto it = std::find_if(files.begin(), files.end(), [&](const auto& f) { return f.first == low; });
    if (it != files.end()) it->second = std::move(data);
    else files.push_back({low, std::move(data)});
  }
  return files;
}

// ---- the looks' label pieces (convert_x360_ui.label_button_relative, label_frame_relative) ----
std::string rel(double v) {
  char buf[96];
  std::snprintf(buf, sizeof buf, "<UnifiedDim scale=\"%.4f\" type=\"Height\" />", v / kLabelHeight720);
  return buf;
}

U32 replace_all(U32 s, const U32& from, const U32& to) {
  for (size_t i = s.find(from); i != U32::npos; i = s.find(from, i + to.size())) s.replace(i, from.size(), to);
  return s;
}

// str.find(sub, start, end), -1 for none (Python's indices: a negative end counts from the end).
long long pyfind(const U32& s, const U32& sub, long long start = 0, long long end = -0x7fffffff) {
  long long n = (long long)s.size();
  if (end == -0x7fffffff) end = n;
  if (end < 0) end += n;
  if (start < 0) start += n;
  start = std::max<long long>(start, 0);
  end = std::min(end, n);
  if (end - start < (long long)sub.size()) return sub.empty() && start <= end ? start : -1;
  const size_t at = s.substr(size_t(start), size_t(end - start)).find(sub);
  return at == U32::npos ? -1 : start + (long long)at;
}

// ---- the 360 code's texts ----
struct Str {
  const char* key;
  uint32_t addr;
};
const Str kStrings[] = {
#include "convert_strings.inc"
};
const Str kStringsUtf8[] = {
    {"pet.nospell", 0x820CFD08},
};

std::vector<std::pair<std::string, std::string>> image_strings(const Bytes& image) {
  std::vector<std::pair<std::string, std::string>> out;
  for (const Str& s : kStrings) {
    const long long off = (long long)s.addr - kImageBase;
    if (!(off >= 0 && off < (long long)image.size() - 1)) continue;
    size_t end = size_t(off);
    while (end + 1 < image.size() && end - size_t(off) < 400 && !(image[end] == 0 && image[end + 1] == 0)) end += 2;
    U32 text;
    try {
      // bytes.decode("utf-16-be") of image[off:end]
      const Bytes b(image.begin() + long(off), image.begin() + long(end));
      Bytes le;
      for (size_t i = 0; i + 1 < b.size(); i += 2) { le.push_back(b[i + 1]); le.push_back(b[i]); }
      if (b.size() % 2) throw Error("odd");
      Bytes withbom = {0xFF, 0xFE};
      withbom.insert(withbom.end(), le.begin(), le.end());
      text = decode_xml(withbom).text;
    } catch (const Error&) {
      continue;
    }
    // Python's `text and len(text) < 100 and text.isprintable() and "=" not in text`, in its order:
    // isprintable only asked of what passes the first two.
    if (text.empty() || text.size() >= 100) continue;
    bool printable = true;
    for (char32_t c : text) printable = printable && py_isprintable(c);
    if (printable && text.find(U'=') == U32::npos) out.push_back({s.key, u8(text)});
  }
  for (const Str& s : kStringsUtf8) {
    const long long off = (long long)s.addr - kImageBase;
    if (!(off >= 0 && off < (long long)image.size())) continue;
    long long end = -1;
    for (long long i = off; i < std::min<long long>(off + 200, (long long)image.size()); i++)
      if (image[size_t(i)] == 0) { end = i; break; }
    U32 text;
    try {
      text = end > off ? from_utf8(std::string(image.begin() + long(off), image.begin() + long(end))) : U32();
    } catch (const Error&) {
      continue;
    }
    // Python's `text and len(text) < 100 and text.isprintable() and "=" not in text`, in its order:
    // isprintable only asked of what passes the first two.
    if (text.empty() || text.size() >= 100) continue;
    bool printable = true;
    for (char32_t c : text) printable = printable && py_isprintable(c);
    if (printable && text.find(U'=') == U32::npos) out.push_back({s.key, u8(text)});
  }
  return out;
}

// ---- Runic's .adm (convert_x360_ui.adm_tree) ----
struct AdmProp {
  std::string key;   // the key's string, or its number when the table lacks it
  uint32_t kind;
  uint64_t value;
};
struct AdmNode {
  std::string name;
  std::vector<AdmProp> props;
  std::vector<AdmNode> children;
};
struct Adm {
  std::map<uint32_t, std::string> table;
  AdmNode root;
};

uint32_t le32(const Bytes& d, size_t o) {
  need(d.size(), o, 4);
  return uint32_t(d[o]) | uint32_t(d[o + 1]) << 8 | uint32_t(d[o + 2]) << 16 | uint32_t(d[o + 3]) << 24;
}

Adm adm_tree(const Bytes& raw, const std::set<uint32_t>& wide) {
  Adm a;
  const uint32_t count = le32(raw, 4);
  size_t o = 8;
  for (uint32_t k = 0; k < count; k++) {
    const uint32_t i = le32(raw, o), n = le32(raw, o + 4);
    o += 8;
    Bytes s = {0xFF, 0xFE};
    const Bytes part = slice(raw, o, size_t(n) * 2);
    s.insert(s.end(), part.begin(), part.end());
    a.table[i] = u8(decode_xml(s).text);   // bytes.decode("utf-16-le")
    o += size_t(n) * 2;
  }
  auto name_of = [&](uint32_t id) { auto it = a.table.find(id); return it != a.table.end() ? it->second : std::to_string(id); };
  std::function<AdmNode(size_t&)> node = [&](size_t& p) {
    AdmNode nd;
    nd.name = name_of(le32(raw, p));
    const uint32_t n = le32(raw, p + 4);
    p += 8;
    for (uint32_t k = 0; k < n; k++) {
      const uint32_t key = le32(raw, p), kind = le32(raw, p + 4);
      p += 8;
      uint64_t v = le32(raw, p);
      if (wide.count(kind)) {
        v |= uint64_t(le32(raw, p + 4)) << 32;
        p += 8;
      } else {
        p += 4;
      }
      nd.props.push_back({name_of(key), kind, v});
    }
    const uint32_t m = le32(raw, p);
    p += 4;
    for (uint32_t k = 0; k < m; k++) nd.children.push_back(node(p));
    return nd;
  };
  a.root = node(o);
  return a;
}

std::map<std::string, std::string> unit_texts(const Bytes& raw) {
  const Adm a = adm_tree(raw, {3, 7});
  std::map<std::string, std::string> out;
  for (const AdmProp& p : a.root.props)
    if ((p.key == "NAME" || p.key == "DESCRIPTION") && (p.kind == 5 || p.kind == 8)) {
      auto it = a.table.find(uint32_t(p.value));
      out[p.key] = it != a.table.end() && p.value <= 0xFFFFFFFFu ? it->second : "";
    }
  return out;
}

std::string spaced(std::string s) {   // .replace("\n", " ").replace("\r", " ")
  for (char& c : s)
    if (c == '\n' || c == '\r') c = ' ';
  return s;
}

std::vector<std::pair<std::string, std::string>> item_descriptions(const std::string& pak, const std::string& pc_pak) {
  std::vector<std::pair<std::string, std::string>> out;
  Zip z3(pak), zp(pc_pak);
  std::map<std::string, std::string> n3;
  auto item_name = [](const std::string& low) { return low.rfind("media/units/items/", 0) == 0 && ends_with(low, ".adm"); };
  for (unsigned i = 0; i < z3.count(); i++) {
    const std::string n = z3.name(i), low = lower_for_filter(n, item_name);
    if (item_name(low)) n3[low] = n;
  }
  for (unsigned i = 0; i < zp.count(); i++) {
    const std::string n = zp.name(i), low = lower_for_filter(n, item_name);
    auto it = n3.find(low);
    if (it == n3.end()) continue;
    std::map<std::string, std::string> pc, x3;
    try {
      pc = unit_texts(zp.read(unsigned(zp.index_of(n))));
      x3 = unit_texts(z3.read(unsigned(z3.index_of(it->second))));
    } catch (const std::exception&) {
      continue;
    }
    const std::string name = pc.count("NAME") ? pc["NAME"] : "";
    const std::string desc = pc.count("DESCRIPTION") ? pc["DESCRIPTION"] : "", d3 = x3.count("DESCRIPTION") ? x3["DESCRIPTION"] : "";
    if (!name.empty() && name.find('=') == std::string::npos && !desc.empty() && desc != d3) {
      const std::string key = "desc." + name;
      auto at = std::find_if(out.begin(), out.end(), [&](const auto& e) { return e.first == key; });
      if (at != out.end()) at->second = "~" + spaced(d3);
      else out.push_back({key, "~" + spaced(d3)});
    }
  }
  return out;
}

std::vector<std::pair<std::string, std::string>> context_tips(const Bytes& raw) {
  const Adm a = adm_tree(raw, {6});
  std::vector<std::pair<std::string, std::string>> out;
  std::function<void(const AdmNode&)> walk = [&](const AdmNode& n) {
    if (n.name == "CONTEXTTIPS")
      for (const AdmProp& p : n.props) {
        auto it = a.table.find(uint32_t(p.value));
        if (p.kind == 8 && p.value <= 0xFFFFFFFFu && it != a.table.end()) {
          auto at = std::find_if(out.begin(), out.end(), [&](const auto& e) { return e.first == p.key; });
          if (at != out.end()) at->second = spaced(it->second);
          else out.push_back({p.key, spaced(it->second)});
        }
      }
    for (const AdmNode& c : n.children) walk(c);
  };
  walk(a.root);
  return out;
}

Bytes globals_of(const std::string& pak) {
  Zip z(pak);
  auto is_globals = [](const std::string& low) { return low == "media/globals.dat.adm"; };
  for (unsigned i = 0; i < z.count(); i++)
    if (is_globals(lower_for_filter(z.name(i), is_globals))) return z.read(i);
  throw Error(pak + " has no media/GLOBALS.DAT.adm");
}

void write_out(const std::string& dest, const std::string& rel_path, const Bytes& data) {
  std::filesystem::path p = fs_path(dest);
  std::string part;
  for (char c : rel_path) {
    if (c == '/') { p /= fs_path(part); part.clear(); }
    else part += c;
  }
  p /= fs_path(part);
  std::filesystem::create_directories(p.parent_path());
  std::ofstream f(p, std::ios::binary);
  if (!f) throw Error("cannot write " + utf8_path(p));
  f.write(reinterpret_cast<const char*>(data.data()), std::streamsize(data.size()));
}

// ---- rename_sets, rename_fonts ----
std::pair<U32, std::vector<U32>> rename_sets(const U32& text_in, const std::vector<std::pair<U32, U32>>& names) {
  std::set<U32> unknown;
  auto lookup = [&](const U32& k) -> const U32* {
    for (const auto& [a, b] : names)
      if (a == k) return &b;
    return nullptr;
  };
  U32 text = sub_all(text_in, find_set_ref, [&](const U32& t, const Match& m) {
    const U32 g2 = substr(t, m.groups[1]);
    if (const U32* n = lookup(g2)) return substr(t, m.groups[0]) + *n;
    unknown.insert(g2);
    return substr(t, m.all);
  });
  text = sub_all(text, [](const U32& t, size_t from) { return find_attr(t, U"imageset", from); },
                 [&](const U32& t, const Match& m) {
                   const U32 g2 = substr(t, m.groups[1]);
                   if (const U32* n = lookup(g2)) return substr(t, m.groups[0]) + *n + substr(t, m.groups[2]);
                   unknown.insert(g2);
                   return substr(t, m.all);
                 });
  return {text, std::vector<U32>(unknown.begin(), unknown.end())};   // sorted (code points)
}

U32 rename_fonts(const U32& text, const std::vector<std::pair<U32, U32>>& fonts) {
  return sub_all(text, find_font_prop, [&](const U32& t, const Match& m) {
    U32 v = substr(t, m.groups[1]);
    for (const auto& [a, b] : fonts)
      if (a == v) { v = b; break; }
    return substr(t, m.groups[0]) + v + substr(t, m.groups[2]);
  });
}

void dict_set(std::vector<std::pair<U32, U32>>& d, const U32& k, const U32& v) {
  for (auto& [a, b] : d)
    if (a == k) { b = v; return; }
  d.push_back({k, v});
}

}  // namespace

size_t count_image_strings(const Bytes& image) { return image_strings(image).size(); }
size_t strings_count() { return sizeof kStrings / sizeof kStrings[0]; }

bool zip_has_name(const std::string& zip_path, const std::string& lower_name) {
  Zip z(zip_path);
  auto is = [&](const std::string& low) { return low == lower_name; };
  for (unsigned i = 0; i < z.count(); i++)
    if (is(lower_for_filter(z.name(i), is))) return true;
  return false;
}

std::vector<std::pair<std::string, uint32_t>> strings_table() {
  std::vector<std::pair<std::string, uint32_t>> out;
  for (const Str& s : kStrings) out.push_back({s.key, s.addr});
  for (const Str& s : kStringsUtf8) out.push_back({std::string("utf8:") + s.key, s.addr});
  return out;
}

U32 label_button_relative(const U32& text) {
  const long long start = pyfind(text, u32(std::string("<WidgetLook name=\"") + kLabelLook + "\">"));
  if (start < 0) return text;
  const long long end = pyfind(text, U"</WidgetLook>", start);
  const long long section = pyfind(text, U"<ImagerySection name=\"Button\">", start, end);
  if (section < 0) return text;
  const long long close = pyfind(text, U"</ImagerySection>", section);
  // text[section:close], Python's slice of a -1 close: up to the last character
  const long long cl = close < 0 ? (long long)text.size() - 1 : close;
  U32 body = cl > section ? text.substr(size_t(section), size_t(cl - section)) : U32();
  const std::pair<const char*, int> dims[] = {{"LeftEdge", -20}, {"TopEdge", -1}, {"Width", 21}, {"Height", 21}};
  for (const auto& [kind, value] : dims)
    body = replace_all(body, u32(std::string("<Dim type=\"") + kind + "\" ><AbsoluteDim value=\"" + std::to_string(value) + "\" /></Dim>"),
                       u32(std::string("<Dim type=\"") + kind + "\" >" + rel(value) + "</Dim>"));
  return text.substr(0, size_t(section)) + body + text.substr(size_t(cl));
}

U32 label_frame_relative(const U32& text) {
  const long long start = pyfind(text, u32(std::string("<WidgetLook name=\"") + kLabelLook + "\">"));
  const long long shared = pyfind(text, U"<WidgetLook name=\"ceguiWidget/StaticShared\"");
  if (start < 0 || shared < 0) return text;
  long long end = pyfind(text, U"</WidgetLook>", start);
  const U32 ref = U"<Section look=\"ceguiWidget/StaticShared\" section=\"frame\" />";
  if (pyfind(text, ref, start, end) < 0) return text;
  const long long sec = pyfind(text, U"<ImagerySection name=\"frame\">", shared, pyfind(text, U"</WidgetLook>", shared));
  if (sec < 0) return text;
  const long long close = pyfind(text, U"</ImagerySection>", sec) + (long long)U32(U"</ImagerySection>").size();
  U32 body = text.substr(size_t(sec), size_t(std::max<long long>(close - sec, 0)));
  body = replace_all(body, U"<ImagerySection name=\"frame\">", U"<ImagerySection name=\"labelframe\">");
  body = replace_all(body, U"<Dim type=\"LeftEdge\" ><AbsoluteDim value=\"-24\" /></Dim>", u32("<Dim type=\"LeftEdge\" >" + rel(-24) + "</Dim>"));
  body = replace_all(body, U"<Dim type=\"TopEdge\" ><AbsoluteDim value=\"-9\" /></Dim>", u32("<Dim type=\"TopEdge\" >" + rel(-9) + "</Dim>"));
  body = replace_all(body, U"<AbsoluteDim value=\"48\" />", u32(rel(48)));
  body = replace_all(body, U"<AbsoluteDim value=\"18\" />", u32(rel(18)));
  const long long e = end < 0 ? (long long)text.size() - 1 : end;
  U32 label = replace_all(text.substr(size_t(start), size_t(std::max<long long>(e - start, 0))), ref, U"<Section section=\"labelframe\" />");
  const long long first_state = pyfind(label, U"<StateImagery");
  if (first_state < 0) return text;
  label = label.substr(0, size_t(first_state)) + body + U"\n" + label.substr(size_t(first_state));
  return text.substr(0, size_t(start)) + label + text.substr(size_t(e));
}

std::vector<std::string> convert_ui(const ConvertOptions& opt) {
  std::vector<std::string> said;
  if (!is_style(opt.style)) throw Error("unknown prompt style " + opt.style);
  const auto files = read_pak(opt.pak);
  auto file_of = [&](const std::string& low) -> const Bytes* {
    for (const auto& [p, d] : files)
      if (p == low) return &d;
    return nullptr;
  };

  // ---- convert(files) ----
  Files out;
  Json report = Json::dict();
  report["imagesets"] = Json::dict();
  report["fonts"] = Json::list();
  report["font_names"] = Json::dict();
  report["layouts"] = Json::dict();
  report["missing_files"] = Json::list();
  std::vector<std::pair<U32, U32>> fonts;   // report["font_names"], kept here and written at the end

  auto put = [&](const std::string& name, const Bytes& data) {
    const std::string path = kDest + name, low = lower_path(path);
    for (const auto& [p, d] : out.items)
      if (lower_path(p) == low) throw Error("two files would be written as " + path);
    out.items.push_back({path, data});
    return path;
  };
  auto take = [&](const std::string& path) -> const Bytes* {
    std::string p = path;
    std::replace(p.begin(), p.end(), '\\', '/');
    const Bytes* d = file_of(lower_path(p));
    if (!d) report["missing_files"].l().push_back(Json::str(path));
    return d;
  };

  const Bytes* scheme_bytes = file_of(kScheme);
  if (!scheme_bytes) throw Error(std::string(kScheme) + " is not in the pak: is this the 360 game's pak.zip?");
  Decoded scheme = decode_xml(*scheme_bytes);

  std::vector<std::pair<U32, U32>> names;
  for (const Match& m : find_tags(scheme.text, U"Imageset")) {
    auto n = attr(substr(scheme.text, m.all), U"Name");
    if (!n) throw Error("an Imageset without a Name in the scheme");
    dict_set(names, *n, kPrefix + *n);
  }
  {
    Json sets = Json::dict();
    for (const auto& [a, b] : names) sets[u8(a)] = Json::str(u8(b));
    report["imagesets"] = sets;
  }

  std::map<std::string, std::string> written;   // lower(src) -> the path written (or src)
  auto need_attr = [](const U32& tag, const U32& name) {
    auto v = attr(tag, name);
    if (!v) throw Error("a tag without " + u8(name));
    return u8(*v);
  };

  auto imageset = [&](const U32& tag_in) -> U32 {
    U32 tag = tag_in;
    const std::string src = need_attr(tag, U"Filename"), key = lower_path(src);
    if (!written.count(key)) {
      const Bytes* data = take(src);
      if (!data) {
        written[key] = src;
        return tag;
      }
      Decoded d = decode_xml(*data);
      auto fix = [&](const U32& t_in) {
        U32 t = t_in;
        if (auto n = attr(t, U"Name")) {
          U32 v = *n;
          for (const auto& [a, b] : names)
            if (a == *n) { v = b; break; }
          t = set_attr(t, U"Name", v);
        }
        const std::string image = need_attr(t, U"Imagefile");
        if (const Bytes* texture = take(image)) t = set_attr(t, U"Imagefile", u32(put(basename(image), *texture)));
        return t;
      };
      written[key] = put(basename(src), encode(retag(d.text, U"Imageset", fix), d.codec));
    }
    const U32 n = *attr(tag, U"Name");
    U32 v;
    for (const auto& [a, b] : names)
      if (a == n) v = b;
    tag = set_attr(tag, U"Name", v);
    return set_attr(tag, U"Filename", u32(written[key]));
  };

  auto font = [&](const U32& tag_in) -> U32 {
    U32 tag = tag_in;
    const std::string src = need_attr(tag, U"Filename"), key = lower_path(src);
    if (!written.count(key)) {
      const Bytes* data = take(src);
      if (!data) {
        written[key] = src;
        return tag;
      }
      Decoded d = decode_xml(*data);
      auto fix = [&](const U32& t_in) {
        U32 t = t_in;
        const U32 n = u32(need_attr(t, U"Name"));
        dict_set(fonts, n, kPrefix + n);
        t = set_attr(t, U"Name", kPrefix + n);
        const std::string ttf = need_attr(t, U"Filename"), tk = lower_path(ttf);
        if (!written.count(tk)) {
          const Bytes* blob = take(ttf);
          written[tk] = blob ? put(basename(ttf), *blob) : ttf;
        }
        return set_attr(t, U"Filename", u32(written[tk]));
      };
      written[key] = put(basename(src), encode(retag(d.text, U"Font", fix), d.codec));
      report["fonts"].l().push_back(Json::str(basename(src)));
    }
    tag = set_attr(tag, U"Filename", u32(written[key]));
    auto n = attr(tag, U"Name");
    return n && !n->empty() ? set_attr(tag, U"Name", kPrefix + *n) : tag;
  };

  auto looknfeel = [&](const U32& tag) -> U32 {
    const std::string src = need_attr(tag, U"Filename");
    const Bytes* data = take(src);
    if (!data) return tag;
    Decoded d = decode_xml(*data);
    auto [text, unknown] = rename_sets(d.text, names);
    text = rename_fonts(text, fonts);
    text = label_button_relative(text);
    text = label_frame_relative(text);
    Json u = Json::list();
    for (const U32& s : unknown) u.l().push_back(Json::str(u8(s)));
    report["looknfeel_unknown_imagesets"] = u;
    return set_attr(tag, U"Filename", u32(put(basename(src), encode(text, d.codec))));
  };

  U32 st = retag(scheme.text, U"GUIScheme", [&](const U32& t) { return set_attr(t, U"Name", kPrefix + u32(need_attr(t, U"Name"))); });
  st = retag(st, U"Imageset", imageset);
  st = retag(st, U"Font", font);
  st = retag(st, U"LookNFeel", looknfeel);
  report["scheme"] = Json::str(put("ceguiWidgets.scheme", encode(st, scheme.codec)));

  std::vector<std::string> layout_paths;
  for (const auto& [p, d] : files)
    if (ends_with(p, ".uilayout") || kExtraLayouts.count(lower_path(basename(p)))) layout_paths.push_back(p);
  std::sort(layout_paths.begin(), layout_paths.end());   // UTF-8 bytes sort as code points
  for (const std::string& path : layout_paths) {
    Decoded d = decode_xml(*file_of(path));
    auto [text, unknown] = rename_sets(d.text, names);
    text = rename_fonts(text, fonts);
    const std::string b = basename(path);
    const std::string name = ends_with(path, ".uilayout") ? b.substr(0, b.size() - 9) + ".layout" : lower_path(b);
    put(name, encode(text, d.codec));
    Json entry = Json::dict();
    entry["windows"] = Json::num((long long)count_window_tags(text));
    Json u = Json::list();
    for (const U32& s : unknown) u.l().push_back(Json::str(u8(s)));
    entry["unknown_imagesets"] = u;
    report["layouts"][name] = entry;
  }
  {
    Json fn = Json::dict();
    for (const auto& [a, b] : fonts) fn[u8(a)] = Json::str(u8(b));
    report["font_names"] = fn;
  }

  // ---- apply_prompts ----
  const auto icons = load_icons(opt.style, opt.swap_confirm_cancel);
  report["prompts"] = Json::dict();
  if (!icons.empty()) {
    std::vector<std::string> sets;
    for (const auto& [p, d] : out.items)
      if (ends_with(p, ".imageset")) sets.push_back(p);
    std::sort(sets.begin(), sets.end());
    for (const std::string& path : sets) {
      const U32 xml = decode_xml(*out.find(path)).text;
      const auto regions = image_regions(xml);
      bool any = false;
      for (const auto& [name, icon] : icons)
        for (const auto& r : regions) any = any || r.first == name;
      if (!any) continue;
      const auto texture = imagefile_value(xml);
      std::string key;
      bool found = false;
      if (texture) {
        const std::string want = lower_path(u8(*texture));
        for (const auto& [p, d] : out.items)
          if (lower_path(p) == want) { key = p; found = true; break; }
      }
      if (!found) {
        report["prompts"][path] = Json::str("its texture is not among the converted files");
        continue;
      }
      try {
        auto [data, count] = patch_texture(xml, *out.find(key), icons);
        out.set(key, std::move(data));
        report["prompts"][path] = Json::num(count);
      } catch (const NonAscii&) {
        throw;
      } catch (const StructError&) {
        throw;   // Python's struct.error: not caught there either
      } catch (const Error& e) {
        report["prompts"][path] = Json::str(e.what());
      }
    }
  }

  // ---- main ----
  for (const auto& [rel_path, data] : out.items) write_out(opt.out, rel_path, data);
  std::string js;
  json_write(js, report, 0);
  write_out(opt.out, "x360_ui_report.json", text_mode(js));
  std::vector<std::pair<std::string, std::string>> texts;
  auto text_set = [&](const std::string& k, const std::string& v) {
    for (auto& [a, b] : texts)
      if (a == k) { b = v; return; }
    texts.push_back({k, v});
  };
  if (!opt.image.empty()) {
    std::ifstream f(fs_path(opt.image), std::ios::binary);
    if (!f) throw Error("cannot open " + opt.image);
    const Bytes image((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    for (const auto& [k, v] : image_strings(image)) text_set(k, v);
    said.push_back(std::to_string(texts.size()) + " texts of the 360's code into x360_strings.txt");
  }
  {
    Zip z(opt.pak);
    const char* shared = "media/sharedtextures/automapoverlay.dds";
    long found = -1;
    auto is_shared = [&](const std::string& low) { return low == shared; };
    for (unsigned i = 0; i < z.count(); i++)
      if (is_shared(lower_for_filter(z.name(i), is_shared))) found = long(i);
    if (found >= 0) write_out(opt.out, shared, z.read(unsigned(z.index_of(z.name(unsigned(found))))));
  }
  if (!opt.pc_pak.empty()) {
    const auto descs = item_descriptions(opt.pak, opt.pc_pak);
    for (const auto& [k, v] : descs) text_set(k, v);
    const auto tips = context_tips(globals_of(opt.pak)), pc_tips = context_tips(globals_of(opt.pc_pak));
    for (const auto& [k, v] : tips) text_set("tip." + k, v);
    for (const auto& [k, v] : pc_tips) text_set("pctip." + k, v);
  }
  if (!texts.empty()) {
    std::sort(texts.begin(), texts.end());
    std::string s;
    for (const auto& [k, v] : texts) s += k + "=" + v + "\n";
    write_out(opt.out, "x360_strings.txt", text_mode(s));
  }
  said.push_back(std::to_string(out.items.size()) + " files into the output");
  return said;
}

}  // namespace x360ui
