#include "game.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#include <sstream>

#include "paths.h"
#include "text_lines.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace x360ui {

extern const char kCeguiExports[];   // proxy/cegui_exports.txt, built in (cmake/embed_blob.cmake)

namespace {

Bytes read_all(const std::string& path, size_t limit = size_t(-1)) {
  std::ifstream f(fs_path(path), std::ios::binary);
  if (!f) throw Error("cannot open " + path);
  Bytes out;
  char buf[65536];
  while (out.size() < limit && f.read(buf, std::streamsize(std::min<size_t>(sizeof buf, limit - out.size())))) out.insert(out.end(), buf, buf + f.gcount());
  if (f.gcount() > 0 && out.size() < limit) out.insert(out.end(), buf, buf + f.gcount());
  return out;
}

bool isfile(const std::string& p) {
  std::error_code ec;
  return !p.empty() && fs::is_regular_file(fs_path(p), ec);
}
bool isdir(const std::string& p) {
  std::error_code ec;
  return !p.empty() && fs::is_directory(fs_path(p), ec);
}
std::string join(const std::string& a, const std::string& b) { return utf8_path(fs_path(a) / fs_path(b)); }

std::string realpath(const std::string& p) {   // os.path.realpath
  std::error_code ec;
  fs::path r = fs::weakly_canonical(fs::absolute(fs_path(p), ec), ec);
  return utf8_path(ec ? fs::absolute(fs_path(p)) : r);
}

std::string lower(std::string s) {
  for (char& c : s) c = char(std::tolower((unsigned char)c));
  return s;
}

// glob.glob of home/PATTERN, '*' a whole or part of one path segment (not matching a leading dot,
// as glob's): the matches in the directories' own order.
bool seg_match(const std::string& pat, const std::string& name) {
  if (!name.empty() && name[0] == '.' && (pat.empty() || pat[0] != '.')) return false;
  // '*' only (the patterns used have no '?' or '[').
  size_t p = 0, n = 0, star = std::string::npos, back = 0;
  while (n < name.size()) {
    if (p < pat.size() && pat[p] == '*') { star = p++; back = n; }
    else if (p < pat.size() && pat[p] == name[n]) { p++; n++; }
    else if (star != std::string::npos) { p = star + 1; n = ++back; }
    else return false;
  }
  while (p < pat.size() && pat[p] == '*') p++;
  return p == pat.size();
}

void glob_into(const fs::path& base, const std::vector<std::string>& segs, size_t i, std::vector<std::string>& out) {
  if (i == segs.size()) {
    std::error_code ec;
    if (fs::exists(base, ec)) out.push_back(utf8_path(base));
    return;
  }
  if (segs[i].find('*') == std::string::npos) return glob_into(base / fs_path(segs[i]), segs, i + 1, out);
  std::error_code ec;
  for (fs::directory_iterator it(base, ec), end; !ec && it != end; it.increment(ec)) {
    const std::string name = utf8_path(it->path().filename());
    if (seg_match(segs[i], name)) glob_into(it->path(), segs, i + 1, out);
  }
}

std::vector<std::string> glob(const std::string& home, const std::string& pattern) {
  std::vector<std::string> segs;
  std::stringstream ss(pattern);
  for (std::string s; std::getline(ss, s, '/');) segs.push_back(s);
  std::vector<std::string> out;
  glob_into(fs_path(home), segs, 0, out);
  return out;
}

bool read_json_file(const std::string& path, JsonValue& out) {
  try {
    const Bytes b = read_all(path);
    return parse_json(std::string(b.begin(), b.end()), out);
  } catch (const Error&) {
    return false;
  }
}

std::string json_str(const JsonValue* v) {   // str(value) for the cases used (a string, or "" for none)
  if (!v) return "";
  if (v->kind == JsonValue::String) return v->s;
  return "";
}

// ---- PE ----
struct Pe {
  const Bytes& d;
  uint32_t nt = 0;
  std::vector<std::array<uint32_t, 4>> sections;   // virtual size, rva, raw size, raw offset
  explicit Pe(const Bytes& data) : d(data) {
    if (d.size() < 0x40 || d[0] != 'M' || d[1] != 'Z') throw Error("not a PE file");
    nt = le32(0x3C);
    if (nt + 24 > d.size() || std::memcmp(d.data() + nt, "PE\0\0", 4) != 0) throw Error("not a PE file");
    const uint16_t n = le16x(nt + 6), opt = le16x(nt + 20);
    for (uint16_t i = 0; i < n; i++) {
      const size_t s = nt + 24 + opt + size_t(i) * 40;
      sections.push_back({le32(s + 8), le32(s + 12), le32(s + 16), le32(s + 20)});
    }
  }
  uint32_t le32(size_t o) const {
    need(d.size(), o, 4);
    return uint32_t(d[o]) | uint32_t(d[o + 1]) << 8 | uint32_t(d[o + 2]) << 16 | uint32_t(d[o + 3]) << 24;
  }
  uint16_t le16x(size_t o) const { return le16(d, o); }
  size_t offset(uint32_t rva) const {
    for (const auto& s : sections)
      if (rva >= s[1] && rva < s[1] + std::max(s[0], s[2])) return s[3] + (rva - s[1]);
    throw Error("an address outside the PE's sections");
  }
  std::string cstr(uint32_t rva) const {
    size_t o = offset(rva), e = o;
    while (e < d.size() && d[e]) e++;
    return std::string(d.begin() + long(o), d.begin() + long(e));
  }
};

}  // namespace

const JsonValue* JsonValue::get(const std::string& key) const {
  if (kind != Object) return nullptr;
  const JsonValue* found = nullptr;
  for (const auto& [k, v] : fields)
    if (k == key) found = &v;   // a repeated key: the last, as json.load
  return found;
}

namespace {
struct JsonParser {
  const std::string& t;
  size_t i = 0;
  void ws() { while (i < t.size() && (t[i] == ' ' || t[i] == '\t' || t[i] == '\n' || t[i] == '\r')) i++; }
  bool lit(const char* s) {
    size_t n = std::strlen(s);
    if (t.compare(i, n, s) != 0) return false;
    i += n;
    return true;
  }
  bool str(std::string& out) {
    if (i >= t.size() || t[i] != '"') return false;
    i++;
    while (i < t.size() && t[i] != '"') {
      char c = t[i++];
      if (c != '\\') { out += c; continue; }
      if (i >= t.size()) return false;
      char e = t[i++];
      if (e == 'u') {
        if (i + 4 > t.size()) return false;
        unsigned u = unsigned(std::stoul(t.substr(i, 4), nullptr, 16));
        i += 4;
        if (u >= 0xD800 && u < 0xDC00 && i + 6 <= t.size() && t[i] == '\\' && t[i + 1] == 'u') {
          unsigned v = unsigned(std::stoul(t.substr(i + 2, 4), nullptr, 16));
          i += 6;
          u = 0x10000 + ((u - 0xD800) << 10) + (v - 0xDC00);
        }
        char32_t cp = u;
        if (cp < 0x80) out += char(cp);
        else if (cp < 0x800) { out += char(0xC0 | (cp >> 6)); out += char(0x80 | (cp & 0x3F)); }
        else if (cp < 0x10000) { out += char(0xE0 | (cp >> 12)); out += char(0x80 | ((cp >> 6) & 0x3F)); out += char(0x80 | (cp & 0x3F)); }
        else { out += char(0xF0 | (cp >> 18)); out += char(0x80 | ((cp >> 12) & 0x3F)); out += char(0x80 | ((cp >> 6) & 0x3F)); out += char(0x80 | (cp & 0x3F)); }
      } else {
        out += e == 'n' ? '\n' : e == 't' ? '\t' : e == 'r' ? '\r' : e == 'b' ? '\b' : e == 'f' ? '\f' : e;
      }
    }
    if (i >= t.size()) return false;
    i++;
    return true;
  }
  bool value(JsonValue& v) {
    ws();
    if (i >= t.size()) return false;
    if (t[i] == '{') {
      v.kind = JsonValue::Object;
      i++;
      ws();
      if (i < t.size() && t[i] == '}') { i++; return true; }
      while (true) {
        ws();
        std::string k;
        if (!str(k)) return false;
        ws();
        if (i >= t.size() || t[i++] != ':') return false;
        JsonValue x;
        if (!value(x)) return false;
        v.fields.push_back({k, x});
        ws();
        if (i < t.size() && t[i] == ',') { i++; continue; }
        if (i < t.size() && t[i] == '}') { i++; return true; }
        return false;
      }
    }
    if (t[i] == '[') {
      v.kind = JsonValue::Array;
      i++;
      ws();
      if (i < t.size() && t[i] == ']') { i++; return true; }
      while (true) {
        JsonValue x;
        if (!value(x)) return false;
        v.items.push_back(x);
        ws();
        if (i < t.size() && t[i] == ',') { i++; continue; }
        if (i < t.size() && t[i] == ']') { i++; return true; }
        return false;
      }
    }
    if (t[i] == '"') { v.kind = JsonValue::String; return str(v.s); }
    if (lit("true")) { v.kind = JsonValue::Bool; v.b = true; return true; }
    if (lit("false")) { v.kind = JsonValue::Bool; return true; }
    if (lit("null")) return true;
    size_t start = i;
    while (i < t.size() && (std::isdigit((unsigned char)t[i]) || t[i] == '-' || t[i] == '+' || t[i] == '.' || t[i] == 'e' || t[i] == 'E')) i++;
    if (i == start) return false;
    v.kind = JsonValue::Number;
    v.n = std::stod(t.substr(start, i - start));
    return true;
  }
};
}  // namespace

bool parse_json(const std::string& text, JsonValue& out) {
  try {
    JsonParser p{text};
    if (!p.value(out)) return false;
    p.ws();
    return p.i == text.size();
  } catch (const std::exception&) {
    return false;
  }
}

bool is_game(const std::string& path) {
  return !path.empty() && isfile(join(path, "Torchlight.exe")) &&
         (isfile(join(path, "CEGUIBase.dll")) || isfile(join(path, "CEGUIBase_orig.dll")));
}

uint32_t exe_timestamp(const std::string& game) {
  const Bytes head = read_all(join(game, "Torchlight.exe"), 4096);
  need(head.size(), 0x3C, 4);
  const uint32_t pe = uint32_t(head[0x3C]) | uint32_t(head[0x3D]) << 8 | uint32_t(head[0x3E]) << 16 | uint32_t(head[0x3F]) << 24;
  need(head.size(), size_t(pe) + 8, 4);
  return uint32_t(head[pe + 8]) | uint32_t(head[pe + 9]) << 8 | uint32_t(head[pe + 10]) << 16 | uint32_t(head[pe + 11]) << 24;
}

std::string build_of(const std::string& game) {
  try {
    const uint32_t s = exe_timestamp(game);
    return s == kGogTimestamp ? "gog" : s == kSteamTimestamp ? "steam" : "unknown";
  } catch (const Error&) {
    return "unknown";
  }
}

bool is_experimental(const std::string& build) { (void)build; return false; }   // none now (install.EXPERIMENTAL)

static std::vector<std::string> steam_libraries(const std::string& home) {
  std::vector<std::string> roots;
  for (const char* p : {".steam/steam", ".steam/debian-installation", ".local/share/Steam", ".var/app/com.valvesoftware.Steam/data/Steam"})
    roots.push_back(join(home, p));
#ifdef _WIN32
  // Where Steam says it is (a Steam installed elsewhere than Program Files), then the defaults.
  const std::pair<HKEY, const wchar_t*> keys[] = {{HKEY_CURRENT_USER, L"Software\\Valve\\Steam"},
                                                  {HKEY_LOCAL_MACHINE, L"SOFTWARE\\WOW6432Node\\Valve\\Steam"}};
  for (const auto& [hive, sub] : keys) {
    HKEY key;
    if (RegOpenKeyExW(hive, sub, 0, KEY_READ, &key) != ERROR_SUCCESS) continue;
    for (const wchar_t* value : {L"SteamPath", L"InstallPath"}) {
      wchar_t path[1024];
      DWORD type = 0, n = sizeof path - sizeof(wchar_t);
      if (RegQueryValueExW(key, value, nullptr, &type, (BYTE*)path, &n) == ERROR_SUCCESS && type == REG_SZ) {
        path[n / sizeof(wchar_t)] = 0;
        roots.push_back(utf8_path(fs::path(path)));
      }
    }
    RegCloseKey(key);
  }
  for (const char* var : {"ProgramFiles(x86)", "ProgramFiles"})
    if (const char* v = std::getenv(var)) roots.push_back(join(v, "Steam"));
#endif
  std::vector<std::string> libs;
  for (const std::string& root : roots) {
    const std::string apps = join(root, "steamapps");
    if (!isdir(apps)) continue;
    libs.push_back(apps);
    try {
      const Bytes b = read_all(join(apps, "libraryfolders.vdf"));
      const std::string t(b.begin(), b.end());
      // "path"\s+"([^"]+)"
      for (size_t k = t.find("\"path\""); k != std::string::npos; k = t.find("\"path\"", k + 1)) {
        size_t j = k + 6, ws = j;
        while (j < t.size() && std::isspace((unsigned char)t[j])) j++;
        if (j == ws || j >= t.size() || t[j] != '"') continue;
        const size_t close = t.find('"', j + 1);
        if (close == std::string::npos || close == j + 1) continue;
        std::string p = t.substr(j + 1, close - j - 1);
        for (size_t q = p.find("\\\\"); q != std::string::npos; q = p.find("\\\\", q + 1)) p.replace(q, 2, "\\");
        libs.push_back(join(p, "steamapps"));
      }
    } catch (const Error&) {
    }
  }
  std::vector<std::string> out;
  for (const std::string& l : libs) {
    const std::string r = realpath(l);
    if (std::find(out.begin(), out.end(), r) == out.end()) out.push_back(r);
  }
  return out;
}

std::vector<std::pair<std::string, std::string>> find_games(const std::string& home) {
  std::vector<std::pair<std::string, std::string>> found;
  auto add = [&](const std::string& store, const std::string& path) {
    if (!is_game(path)) return;
    const auto e = std::make_pair(store, realpath(path));
    if (std::find(found.begin(), found.end(), e) == found.end()) found.push_back(e);
  };
  JsonValue heroic;
  if (read_json_file(join(home, ".config/heroic/gog_store/installed.json"), heroic))
    if (const JsonValue* inst = heroic.get("installed"); inst && inst->kind == JsonValue::Array)
      for (const JsonValue& g : inst->items) add("gog", json_str(g.get("install_path")));
  for (const char* pattern : {"GOG Games/Torchlight", "Games/*/Torchlight", "Games/Heroic/Torchlight",
                              "*/drive_c/GOG Games/Torchlight", "*/*/drive_c/GOG Games/Torchlight",
                              "*/*/*/drive_c/GOG Games/Torchlight", ".wine/drive_c/GOG Games/Torchlight",
                              "Games/*/drive_c/GOG Games/Torchlight"})
    for (const std::string& p : glob(home, pattern)) add("gog", p);
#ifdef _WIN32
  add("gog", "C:\\GOG Games\\Torchlight");
  if (const char* pf = std::getenv("ProgramFiles(x86)")) add("gog", join(pf, "GOG Galaxy\\Games\\Torchlight"));
  HKEY key;
  if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\WOW6432Node\\GOG.com\\Games", 0, KEY_READ, &key) == ERROR_SUCCESS) {
    wchar_t sub[256];
    for (DWORD i = 0;; i++) {
      DWORD n = 256;
      if (RegEnumKeyExW(key, i, sub, &n, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS) break;
      HKEY g;
      if (RegOpenKeyExW(key, sub, 0, KEY_READ, &g) != ERROR_SUCCESS) continue;
      wchar_t name[512], path[1024];
      DWORD nn = sizeof name, pn = sizeof path;
      if (RegQueryValueExW(g, L"gameName", nullptr, nullptr, (BYTE*)name, &nn) == ERROR_SUCCESS &&
          RegQueryValueExW(g, L"path", nullptr, nullptr, (BYTE*)path, &pn) == ERROR_SUCCESS) {
        std::wstring nm(name);
        for (auto& c : nm) c = wchar_t(towlower(c));
        if (nm.find(L"torchlight") != std::wstring::npos) add("gog", utf8_path(fs::path(path)));
      }
      RegCloseKey(g);
    }
    RegCloseKey(key);
  }
#endif
  for (const std::string& apps : steam_libraries(home))
    if (isfile(join(apps, "appmanifest_41500.acf"))) add("steam", join(join(apps, "common"), "Torchlight"));
  for (const char* cfg : {".config/legendary/installed.json", ".config/heroic/legendaryConfig/legendary/installed.json"}) {
    JsonValue data;
    if (!read_json_file(join(home, cfg), data) || data.kind != JsonValue::Object) continue;
    for (const auto& [k, g] : data.fields)
      if (g.kind == JsonValue::Object && lower(json_str(g.get("title"))).find("torchlight") != std::string::npos)
        add("epic", json_str(g.get("install_path")));
  }
#ifdef _WIN32
  if (const char* pd = std::getenv("ProgramData")) {
    std::error_code ec;
    for (fs::directory_iterator it(fs::path(pd) / "Epic/EpicGamesLauncher/Data/Manifests", ec), end; !ec && it != end; it.increment(ec)) {
      if (it->path().extension() != ".item") continue;
      JsonValue g;
      if (read_json_file(utf8_path(it->path()), g) && lower(json_str(g.get("DisplayName"))).find("torchlight") != std::string::npos)
        add("epic", json_str(g.get("InstallLocation")));
    }
  }
#endif
  return found;
}

bool is_proxy(const std::string& dll_path) {
  const Bytes b = read_all(dll_path);
  static const char needle[] = "CEGUIBase_orig.";
  return std::search(b.begin(), b.end(), needle, needle + sizeof needle - 1) != b.end();
}

std::vector<std::pair<std::string, uint32_t>> pe_exports(const Bytes& data) {
  Pe pe(data);
  const uint16_t magic = pe.le16x(pe.nt + 24);
  const size_t dirs = pe.nt + 24 + (magic == 0x20B ? 112 : 96);
  const uint32_t rva = pe.le32(dirs);
  std::vector<std::pair<std::string, uint32_t>> out;
  if (!rva) return out;
  const size_t o = pe.offset(rva);
  const uint32_t base = pe.le32(o + 16), nnames = pe.le32(o + 24), names = pe.le32(o + 32), ords = pe.le32(o + 36);
  for (uint32_t i = 0; i < nnames; i++) {
    const std::string name = pe.cstr(pe.le32(pe.offset(names) + 4 * size_t(i)));
    const uint16_t index = pe.le16x(pe.offset(ords) + 2 * size_t(i));
    out.push_back({name, base + index});
  }
  return out;
}

std::vector<std::pair<std::string, uint32_t>> exports_list() {
  std::vector<std::pair<std::string, uint32_t>> out;
  for (const std::string& line : text_lines(std::string(kCeguiExports))) {
    if (line.empty() || line[0] == '#') continue;
    std::stringstream l(line);
    uint32_t ord;
    std::string source, name;
    l >> ord >> source >> name;
    out.push_back({name, ord});
  }
  return out;
}

void check_real_dll(const std::string& game) {
  const std::string dll = join(game, "CEGUIBase.dll"), orig = join(game, "CEGUIBase_orig.dll");
  const std::string real = isfile(dll) && !is_proxy(dll) ? dll : orig;
  if (!isfile(real) || is_proxy(real)) throw Error(game + ": the real CEGUIBase.dll is missing: reinstall the game's files.");
  auto have = pe_exports(read_all(real));
  auto want = exports_list();
  std::set<std::pair<std::string, uint32_t>> a(have.begin(), have.end()), b(want.begin(), want.end());
  size_t diff = 0;
  for (const auto& e : a) diff += !b.count(e);
  for (const auto& e : b) diff += !a.count(e);
  if (diff) throw Error(real + " is not the CEGUIBase.dll this mod is made for (" + std::to_string(diff) + " exports differ).");
}

}  // namespace x360ui
