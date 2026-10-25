#include "install.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <set>
#include <thread>

#include "bytes.h"
#include "convert.h"
#include "game.h"
#include "paths.h"
#include "stfs.h"
#include "text_lines.h"
#include "xex.h"

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace x360ui {

// The proxies built into the executable by the release (cmake -DX360UI_PROXY_GOG=..., _STEAM=...):
// empty when this build has none.
extern const unsigned char* const kProxyGog;
extern const size_t kProxyGogSize;
extern const unsigned char* const kProxySteam;
extern const size_t kProxySteamSize;
extern const char kLicenseText[];   // LICENSE, NOTICES.md and LGPL-2.1.txt, built in (CMakeLists.txt)
extern const char kNoticesText[];
extern const char kLgplText[];
extern const char kOflText[];
extern const char kSdlNoticesText[];

namespace {

// What a rollback could not undo, added to the last message (an elevated run's console closes).
std::string g_notes;
// In an undo folder whose originals a rollback could not put back: uninstall leaves it.
const char* const kKeepMark = "x360ui-keep.txt";

const char* const kUsage =
    "x360ui-install [install] [--package FILE | --x360-dir DIR] [--game DIR] [--bag pc|360] [--yes] [--console]\n"
    "x360ui-install set-bag --bag pc|360 [--game DIR] [--yes] [--console]   (an installed game's bag, no package)\n"
    "               [--dll FILE]\n"
    "x360ui-install uninstall [--game DIR] [--yes] [--console] [--anyway]\n"
    "x360ui-install list\n"
    "x360ui-install notices\n";
const char* const kStyles[] = {"playstation3", "playstation4", "playstation5", "nintendo", "nintendo-swapped"};
const uint32_t kX360Title = 0x58410A7E, kX360ContentType = 0x000D0000;
const char* const kX360Layout = "media/ui/mastermenu_xenon.layout";
const std::string kNoModWarning =
    "Without the mod (uninstalled, or removed by Steam or GOG when they verify, repair or update the "
    "game) the original game deletes the items that do not fit PC's bag. Before uninstalling, install "
    "PC's bag, then load and save each character.";
const std::string kNoSavesFound =
    "No save folder of this game was found to back up (a Steam install's saves are in Steam's "
    "prefix, left alone; Heroic's prefix may be elsewhere). Back up your saves yourself first.";
const std::string kNoModWarningAfter =
    "A character not loaded and saved with PC's bag since keeps its items where the 360's bag put "
    "them, and without the mod the original game deletes those that do not fit PC's bag.";
const std::string kBags =
    "The 360's bag has one section of 50 slots for every kind of item; PC's has three of 21 (any "
    "item, spells, fish).\n\n"
    "When a character changes bag, the mod moves the items that do not fit to free slots or to the "
    "pet's bag; if some still do not fit, the game says so and closes without saving.";
const std::string kBagWarning = kBags + "\n\n" + kNoModWarning + "\n\nThe saves found are backed up first.\n\nUse the 360's bag?";
const char* const kTitle = "Torchlight: the Xbox 360 interface";

struct Exit : Error {   // Python's SystemExit with a message: printed, status 1
  using Error::Error;
};

fs::path P(const std::string& s) { return fs_path(s); }
std::string S(const fs::path& p) { return utf8_path(p); }
bool is_file(const fs::path& p) { std::error_code ec; return fs::is_regular_file(p, ec); }
bool is_dir(const fs::path& p) { std::error_code ec; return fs::is_directory(p, ec); }
bool path_exists(const fs::path& p) { std::error_code ec; return fs::exists(p, ec); }

Bytes read_file(const fs::path& p) {
  std::ifstream f(p, std::ios::binary);
  if (!f) throw Exit("cannot read " + S(p));
  return Bytes((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}
void write_file(const fs::path& p, const Bytes& d) {
  std::ofstream f(p, std::ios::binary);
  if (!f || !f.write(reinterpret_cast<const char*>(d.data()), std::streamsize(d.size()))) throw Exit("cannot write " + S(p));
}
bool same_file(const fs::path& a, const fs::path& b) { return read_file(a) == read_file(b); }

std::string home_dir() {
#ifdef _WIN32
  if (const char* h = std::getenv("USERPROFILE")) return h;
#endif
  if (const char* h = std::getenv("HOME")) return h;
  return ".";
}

// ---- questions, on the console (the wizard asks in its window; Python's Ask) ----
class Ask {
 public:
  Ask(bool console, bool yes) : yes_(yes) { (void)console; }
  std::string file(const std::string& title) {
    if (yes_) return "";
    std::cout << title << ": " << std::flush;
    std::string line;
    if (!std::getline(std::cin, line)) return "";
    trim(line);
    if (line.size() >= 2 && line.front() == '"' && line.back() == '"') line = line.substr(1, line.size() - 2);
    return line;
  }
  bool yes_no(const std::string& text, bool def) {
    if (yes_) return def;
    std::cout << text << (def ? " [Y/n] " : " [y/N] ") << std::flush;
    std::string a;
    if (!std::getline(std::cin, a)) return def;
    trim(a);
    if (a.empty()) return def;
    return a[0] == 'y' || a[0] == 'Y';
  }
  void info(const std::string& text) { std::cout << text << "\n"; }
  void error(const std::string& text) { std::cerr << text << "\n"; }

 private:
  bool yes_;
  static void trim(std::string& s) {
    while (!s.empty() && std::isspace((unsigned char)s.back())) s.pop_back();
    size_t i = 0;
    while (i < s.size() && std::isspace((unsigned char)s[i])) i++;
    s = s.substr(i);
  }
};

// ---- the 360 copy ----
bool is_x360_pak(const fs::path& path);
std::string check_package(const StfsPackage& pkg) {
  char buf[200];
  if (pkg.title_id != kX360Title) {
    std::snprintf(buf, sizeof buf, "This package is of another game (title %08X), not Torchlight's (%08X).", pkg.title_id, kX360Title);
    return buf;
  }
  if (pkg.content_type != kX360ContentType) {
    std::snprintf(buf, sizeof buf, "This package is not the game itself (content type %08X).", pkg.content_type);
    return buf;
  }
  return "";
}

std::string lower_ascii(std::string s) {
  for (char& c : s) c = char(std::tolower((unsigned char)c));
  return s;
}

// The first pak.zip, default.xex and default.bin of a folder extracted from the 360 package, as
// os.walk finds them: each folder's files, then its subfolders, top down (install.read_360_copy).
std::map<std::string, fs::path> files_of_360_folder(const fs::path& folder) {
  std::map<std::string, fs::path> found;
  std::function<void(const fs::path&)> walk = [&](const fs::path& d) {
    std::vector<fs::path> subs;
    std::error_code ec;
    for (fs::directory_iterator it(d, ec), end; !ec && it != end; it.increment(ec)) {
      if (it->is_directory(ec)) { subs.push_back(it->path()); continue; }
      const std::string low = lower_ascii(S(it->path().filename()));
      if ((low == "pak.zip" || low == "default.xex" || low == "default.bin") && !found.count(low)) found[low] = it->path();
    }
    for (const auto& s : subs) walk(s);
  };
  walk(folder);
  return found;
}

// (pak.zip, decrypted image) of the 360 copy into work (install.read_360_copy).
std::pair<fs::path, fs::path> read_360_copy(std::string source, const fs::path& work) {
  const fs::path pak = work / "pak.zip", xex = work / "default.xex";
  fs::path image;
  if (is_file(P(source))) {
    std::ifstream f(P(source), std::ios::binary);
    char magic[4] = {};
    f.read(magic, 4);
    const std::string m(magic, size_t(f.gcount()));
    if (m != "CON " && m != "LIVE" && m != "PIRS") source = S(P(source).parent_path());   // a file of an extracted folder
  }
  if (is_file(P(source))) {
    std::unique_ptr<StfsPackage> pkg;
    try {
      pkg = std::make_unique<StfsPackage>(source);
    } catch (const Error& e) {
      throw Exit("Cannot read " + source + " as an Xbox 360 package: " + e.what());
    }
    const std::string problem = check_package(*pkg);
    if (!problem.empty()) throw Exit(problem);
    for (const auto& [name, dest] : {std::make_pair("pak.zip", pak), std::make_pair("default.xex", xex)}) {
      const StfsEntry* e = pkg->find(name);
      if (!e) throw Exit(std::string("The package has no ") + name + ".");
      std::ofstream out(dest, std::ios::binary);
      if (!out) throw Exit("cannot write " + S(dest));
      try {
        pkg->read_to(*e, out);
      } catch (const Error& err) {
        throw Exit(std::string("Cannot read ") + name + " from the package: " + err.what());
      }
      out.close();
      if (!out) throw Exit("cannot write " + S(dest));
    }
  } else if (is_dir(P(source))) {
    std::map<std::string, fs::path> found = files_of_360_folder(P(source));
    if (!found.count("pak.zip")) throw Exit("No pak.zip in " + source + ".");
    fs::copy_file(found["pak.zip"], pak, fs::copy_options::overwrite_existing);
    if (found.count("default.bin")) image = found["default.bin"];
    else if (found.count("default.xex")) fs::copy_file(found["default.xex"], xex, fs::copy_options::overwrite_existing);
    else throw Exit("No default.xex in " + source + ".");
  } else {
    throw Exit(source + " does not exist.");
  }
  if (!is_x360_pak(pak)) throw Exit("The pak.zip found is not the Xbox 360 game's.");
  if (image.empty()) {
    std::cout << "decrypting the 360's default.xex\n";
    XexImage x;
    try {
      x = xex_dump(read_file(xex));
    } catch (const Error& e) {
      throw Exit(std::string("Cannot decrypt default.xex: ") + e.what());
    }
    image = work / "default.bin";
    write_file(image, x.image);
  }
  const Bytes data = read_file(image);
  if (!(data.size() >= 2 && data[0] == 'M' && data[1] == 'Z' && count_image_strings(data) >= strings_count() / 2))
    throw Exit("The 360's executable is not Torchlight's Arcade build 1.0.140.0.");
  return {pak, image};
}

// ---- saves (install.save_folders, back_up_saves) ----
std::vector<fs::path> save_folders(const fs::path& game) {
  const fs::path tail = fs::path("AppData") / "Roaming" / "runic games" / "torchlight" / "save";
  std::vector<fs::path> found;
#ifdef _WIN32
  if (const char* a = std::getenv("APPDATA")) found.push_back(fs::path(a) / "runic games" / "torchlight" / "save");
#endif
  std::error_code ec;
  fs::path up = fs::weakly_canonical(fs::absolute(game), ec);
  while (up.has_parent_path() && up.parent_path() != up && up.filename() != "drive_c") up = up.parent_path();
  std::vector<fs::path> roots;
  if (up.filename() == "drive_c") {
    roots.push_back(up);
    const fs::path beside = up.parent_path().parent_path();
    const std::string h = S(fs::weakly_canonical(fs::absolute(P(home_dir())), ec)), b = S(fs::weakly_canonical(beside, ec));
    if (h.rfind(b, 0) != 0) {   // not the home or above it (~/.wine: every prefix of the home)
      for (fs::directory_iterator it(beside, ec), end; !ec && it != end; it.increment(ec)) {
        if (it->path().filename().string()[0] == '.') continue;
        if (is_dir(it->path() / "drive_c")) roots.push_back(it->path() / "drive_c");
        std::error_code ec2;
        for (fs::directory_iterator jt(it->path(), ec2), e2; !ec2 && jt != e2; jt.increment(ec2))
          if (jt->path().filename().string()[0] != '.' && is_dir(jt->path() / "drive_c")) roots.push_back(jt->path() / "drive_c");
      }
    }
  }
  for (const fs::path& root : roots) {
    std::error_code ec2;
    for (fs::directory_iterator it(root / "users", ec2), end; !ec2 && it != end; it.increment(ec2))
      if (it->path().filename().string()[0] != '.') found.push_back(it->path() / tail);
  }
  std::vector<fs::path> out;
  for (const fs::path& p : found) {
    const fs::path r = fs::weakly_canonical(p, ec);
    bool compat = false;
    for (const auto& part : r) compat = compat || part == "compatdata";
    if (is_dir(r) && !compat && std::find(out.begin(), out.end(), r) == out.end()) out.push_back(r);
  }
  return out;
}

std::vector<fs::path> back_up_saves(const std::vector<fs::path>& folders) {
  char stamp[32];
  std::time_t now = std::time(nullptr);
  std::strftime(stamp, sizeof stamp, "%Y%m%d-%H%M%S", std::localtime(&now));
  std::vector<fs::path> copies;
  for (const fs::path& f : folders) {
    fs::path copy = S(f) + "-backup-" + stamp;
    for (int n = 2; path_exists(copy); n++) copy = S(f) + "-backup-" + stamp + "-" + std::to_string(n);   // one second, two backups
    fs::copy(f, copy, fs::copy_options::recursive);
    // shutil.copytree's copy2: the files' and folders' times kept (folders after their files).
    std::vector<fs::path> dirs = {fs::path()};
    for (fs::recursive_directory_iterator it(f), end; it != end; ++it) {
      const fs::path rel = fs::relative(it->path(), f);
      if (it->is_directory()) dirs.push_back(rel);
      else {
        fs::last_write_time(copy / rel, fs::last_write_time(it->path()));   // the modes: copy_file keeps them
      }
    }
    for (auto d = dirs.rbegin(); d != dirs.rend(); ++d) fs::last_write_time(copy / *d, fs::last_write_time(f / *d));
    copies.push_back(copy);
  }
  return copies;
}

// ---- the ini (install.read_ini, write_ini) ----
// The ini's lines as Python reads them: UTF-8 with errors="ignore" (every byte outside a valid
// sequence dropped), universal newlines (\r\n, \r and \n end a line), each line without its end.
std::vector<U32> ini_lines(const fs::path& game) {
  const fs::path path = game / "x360_ui.ini";
  if (!is_file(path)) return {};
  const Bytes b = read_file(path);
  U32 text;
  for (size_t i = 0; i < b.size();) {
    const unsigned c = b[i];
    size_t n = c < 0x80 ? 1 : c >= 0xC2 && c <= 0xDF ? 2 : c >= 0xE0 && c <= 0xEF ? 3 : c >= 0xF0 && c <= 0xF4 ? 4 : 0;
    bool ok = n > 0 && i + n <= b.size();
    for (size_t k = 1; ok && k < n; k++) ok = (b[i + k] & 0xC0) == 0x80;
    if (ok && n >= 3) {   // no overlong form, no surrogate, nothing past U+10FFFF
      const unsigned d = b[i + 1];
      ok = !(c == 0xE0 && d < 0xA0) && !(c == 0xED && d > 0x9F) && !(c == 0xF0 && d < 0x90) && !(c == 0xF4 && d > 0x8F);
    }
    if (!ok) { i++; continue; }
    char32_t v = n == 1 ? c : c & (0xFF >> (n + 1));
    for (size_t k = 1; k < n; k++) v = (v << 6) | (b[i + k] & 0x3F);
    text += v;
    i += n;
  }
  return text_lines(text);
}

U32 py_strip(const U32& s) {   // str.strip()
  size_t a = 0, z = s.size();
  while (a < z && py_isspace(s[a])) a++;
  while (z > a && py_isspace(s[z - 1])) z--;
  return s.substr(a, z - a);
}

std::string utf8(const U32& s) {
  const Bytes b = encode(s, Codec::Utf8);
  return std::string(b.begin(), b.end());
}

std::map<std::string, std::string> read_ini(const fs::path& game) {
  std::map<std::string, std::string> v;
  for (const U32& line : ini_lines(game)) {
    const size_t eq = line.find(U'=');
    if (eq != U32::npos) v[utf8(py_strip(line.substr(0, eq)))] = utf8(py_strip(line.substr(eq + 1)));
  }
  return v;
}

std::string wine_path(const fs::path& p) {
#ifdef _WIN32
  return S(p);
#else
  std::string s = S(fs::absolute(p).lexically_normal());
  std::replace(s.begin(), s.end(), '/', '\\');
  return "Z:" + s;
#endif
}

Bytes ini_text(const fs::path& game, const fs::path& out, const std::string& bag) {
  // Remembered once the 360's bag was used, until uninstall: a character not yet loaded and saved
  // with PC's bag would lose what does not fit it.
  auto old = read_ini(game);
  const bool used = bag == "360" || old["bag"] == "360" || old["bag_360_used"] == "1";
  std::vector<std::pair<std::string, std::string>> owned = {
      {"mode", "scheme"}, {"out", wine_path(out)}, {"pad", "1"}, {"pad_mode", "direct"}, {"prompts", "auto"}, {"bag", bag}};
  if (used) owned.push_back({"bag_360_used", "1"});
  std::set<std::string> keys = {"mode", "out", "pad", "pad_mode", "prompts", "bag", "bag_360_used", "update_check"};
  std::vector<U32> kept;
  for (const U32& line : ini_lines(game)) {
    const std::string key = utf8(py_strip(line.substr(0, line.find(U'='))));
    // The proxy's test options (runs on copies): never kept by an install (install.is_test_option).
    const bool test = key == "save_root" || key == "hold_saves" || key == "pad_detect" || key == "console" ||
                      key.rfind("test_", 0) == 0;
    if (!keys.count(key) && !test) kept.push_back(line);
  }
  std::string t;
  for (const auto& [k, v] : owned) t += k + "=" + v + "\r\n";
  for (const U32& l : kept)
    if (!py_strip(l).empty()) t += utf8(l) + "\r\n";
  return Bytes(t.begin(), t.end());
}

// ---- the changes to the game's folder as one step (the owner: undone if a part fails) ----
class Change {
 public:
  explicit Change(const fs::path& game) : game_(game), undo_(game / ("x360ui-undo-" + std::to_string(rand_id()))) {}
  ~Change() {
    if (!done_) rollback();
  }
  // A file or folder put aside (moved into the undo folder), to come back on a rollback.
  void put_aside(const fs::path& p) {
    if (!path_exists(p)) return;
    fs::create_directories(undo_);
    const fs::path to = undo_ / std::to_string(steps_.size());
    fs::rename(p, to);
    steps_.push_back({Step::Aside, p, to});
  }
  void rename(const fs::path& from, const fs::path& to) {
    fs::rename(from, to);
    steps_.push_back({Step::Renamed, from, to});
  }
  void write(const fs::path& p, const Bytes& d) {
    put_aside(p);
    steps_.push_back({Step::Created, p, {}});   // before writing: a partial file is undone too
    write_file(p, d);
  }
  void copy(const fs::path& from, const fs::path& to) {
    put_aside(to);
    steps_.push_back({Step::Created, to, {}});
    fs::copy_file(from, to);
  }
  const fs::path& undo_folder() const { return undo_; }
  void commit() {
    done_ = true;
    std::error_code ec;
    fs::remove_all(undo_, ec);
    if (ec && path_exists(undo_))   // a file in use (Windows): left for uninstall, which removes x360ui-undo-*
      std::cerr << "could not remove " << S(undo_) << " (" << ec.message() << "); it can be deleted by hand\n";
  }
  void rollback() {
    done_ = true;
    for (auto it = steps_.rbegin(); it != steps_.rend(); ++it) {
      std::error_code ec;
      if (it->kind == Step::Created) fs::remove_all(it->a, ec);
      else if (std::getenv("X360UI_TEST_FAIL_UNDO")) ec = std::make_error_code(std::errc::device_or_resource_busy);
      else if (path_exists(it->a)) ec = std::make_error_code(std::errc::file_exists);   // a failed step left it: not overwritten
      else fs::rename(it->b, it->a, ec);
      if (ec) {
        failed_ = true;
        g_notes += "\ncould not undo a change to " + S(it->a) + " (" + ec.message() + "); it is at " +
                   S(it->kind == Step::Created ? it->a : it->b);
      }
    }
    std::error_code ec;
    if (!failed_ || !path_exists(undo_) || fs::is_empty(undo_, ec)) {
      fs::remove_all(undo_, ec);
      return;
    }
    // What could not be put back is still in the undo folder: kept, marked so uninstall leaves it.
    std::ofstream mark(undo_ / kKeepMark);
    mark << "The originals that could not be put back, in the order they were moved here.\n";
    mark.close();
    g_notes += "\n" + S(undo_) + " holds the originals: put them back by hand" +
               (mark ? "." : ", before uninstalling (it could not be marked to be kept).");
  }

 private:
  struct Step {
    enum Kind { Aside, Renamed, Created } kind;
    fs::path a, b;
  };
  static unsigned rand_id() { return unsigned(std::chrono::steady_clock::now().time_since_epoch().count() & 0xFFFFFF); }
  fs::path game_, undo_;
  std::vector<Step> steps_;
  bool done_ = false, failed_ = false;
};

// build_proxy.install, as steps of a Change.
void install_proxy(Change& ch, const fs::path& game, const fs::path& proxy) {
  const fs::path dll = game / "CEGUIBase.dll", orig = game / "CEGUIBase_orig.dll";
  if (!is_proxy(S(proxy))) throw Exit(S(proxy) + " is not the proxy; not installing it");
  if (!path_exists(dll) && path_exists(orig)) ch.copy(orig, dll);   // the proxy's place empty: the real one put back first
  if (path_exists(orig) && !is_proxy(S(dll)) && same_file(dll, orig)) ch.put_aside(orig);   // a store put the real one back
  if (!path_exists(orig)) {
    if (is_proxy(S(dll))) throw Exit(S(dll) + " is a proxy and " + S(orig) + " is missing: restore the real DLL first");
    ch.rename(dll, orig);
  } else if (!is_proxy(S(dll))) {
    throw Exit("both " + S(dll) + " and " + S(orig) + " are real DLLs; look before overwriting");
  }
  ch.copy(proxy, dll);
  std::cout << "installed the proxy in " << S(game) << "\n";
}

// build_proxy.remove: true once the real DLL is in place.
bool remove_proxy(Change& ch, const fs::path& game) {
  const fs::path dll = game / "CEGUIBase.dll", orig = game / "CEGUIBase_orig.dll";
  if (path_exists(orig) && (!path_exists(dll) || same_file(dll, orig))) {
    ch.put_aside(dll);
    ch.rename(orig, dll);
    std::cout << "restored the real CEGUIBase.dll in " << S(game) << "\n";
    return true;
  }
  if (!path_exists(orig)) {
    std::cout << "no proxy installed\n";
    return path_exists(dll) && !is_proxy(S(dll));
  }
  if (!is_proxy(S(dll))) throw Exit(S(dll) + " is not the proxy; not touching it");
  ch.put_aside(dll);
  ch.rename(orig, dll);
  std::cout << "restored the real CEGUIBase.dll in " << S(game) << "\n";
  return true;
}

std::vector<fs::path> converted_folders(const fs::path& game) {
  std::vector<fs::path> out = {game / "x360ui"};
  for (const char* s : kStyles) out.push_back(game / (std::string("x360ui-") + s));
  return out;
}

// ---- permissions (Windows: UAC only when the folder needs it; nothing ever redirected) ----
bool can_write(const fs::path& dir) {
#ifdef _WIN32
  const fs::path probe = dir / ("x360ui-write-test-" + std::to_string(GetCurrentProcessId()));
  HANDLE h = CreateFileW(probe.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
  if (h == INVALID_HANDLE_VALUE) return false;
  CloseHandle(h);
  return true;
#else
  return access(dir.c_str(), W_OK) == 0;
#endif
}

#ifdef _WIN32
bool is_elevated() {
  HANDLE token;
  if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return false;
  TOKEN_ELEVATION e{};
  DWORD n = 0;
  const bool ok = GetTokenInformation(token, TokenElevation, &e, sizeof e, &n) && e.TokenIsElevated;
  CloseHandle(token);
  return ok;
}

std::wstring quote(const std::string& a) {
  std::wstring w = fs_path(a).wstring(), out = L"\"";
  for (size_t i = 0; i < w.size(); i++) {
    size_t bs = 0;
    while (i < w.size() && w[i] == L'\\') { bs++; i++; }
    if (i == w.size()) { out.append(bs * 2, L'\\'); break; }
    if (w[i] == L'"') out.append(bs * 2 + 1, L'\\');
    else out.append(bs, L'\\');
    out += w[i];
  }
  return out + L"\"";
}

// The same installer again, elevated, with the answers already given; its exit status, or -1 when
// the elevation was declined (or a test asks for that: X360UI_NO_ELEVATE).
int run_elevated(const std::vector<std::string>& args, const std::function<void()>& poll) {
  if (std::getenv("X360UI_NO_ELEVATE")) return -1;
  wchar_t self[MAX_PATH];
  GetModuleFileNameW(nullptr, self, MAX_PATH);
  std::wstring params;
  for (const std::string& a : args) params += quote(a) + L" ";
  SHELLEXECUTEINFOW sei{};
  sei.cbSize = sizeof sei;
  sei.fMask = SEE_MASK_NOCLOSEPROCESS;
  // X360UI_TEST_ELEVATE_AS_USER: the same second run, without UAC (the tests').
  sei.lpVerb = std::getenv("X360UI_TEST_ELEVATE_AS_USER") ? L"open" : L"runas";
  sei.lpFile = self;
  sei.lpParameters = params.c_str();
  std::error_code ec;
  const std::wstring cwd = fs::current_path(ec).wstring();
  if (!ec) sei.lpDirectory = cwd.c_str();
  sei.nShow = SW_SHOWNORMAL;
  if (!ShellExecuteExW(&sei)) return -1;   // ERROR_CANCELLED: the user said no
  while (WaitForSingleObject(sei.hProcess, 100) == WAIT_TIMEOUT) poll();
  poll();
  DWORD code = 1;
  GetExitCodeProcess(sei.hProcess, &code);
  CloseHandle(sei.hProcess);
  return int(code);
}
#endif

// The elevated run's last message (its result or its error), written where the first run reads it.
std::string g_result_file;

// The progress: given to the wizard (set_progress) and, in an elevated run, written to the file
// the first run reads (one line, "fraction<TAB>text", replaced each time).
Progress g_progress;
std::string g_progress_file;

void progress(double fraction, const std::string& text) {
  if (g_progress) g_progress(fraction, text);
  if (!g_progress_file.empty()) {
    char f[16];
    std::snprintf(f, sizeof f, "%.4f", fraction);
    const fs::path tmp = P(g_progress_file + ".new");
    {
      std::ofstream out(tmp, std::ios::binary);
      out << f << "\t" << text << "\n";
    }
    std::error_code ec;
    fs::rename(tmp, P(g_progress_file), ec);   // whole lines only for the reader
    if (ec) {                                  // the reader had it open: once more
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
      fs::rename(tmp, P(g_progress_file), ec);
    }
  }
}

// The elevated run's progress line, given to this run's progress.
[[maybe_unused]] void forward_progress(const fs::path& file) {
  // Never throws: the elevated run goes on whatever happens to its progress line here.
  std::ifstream f(file, std::ios::binary);
  if (!f) return;
  const std::string all((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
  const std::vector<std::string> lines = text_lines(all);
  if (lines.empty()) return;
  const std::string& line = lines[0];
  const size_t tab = line.find('\t');
  if (tab == std::string::npos) return;
  const std::string text = line.substr(tab + 1);
  try {
    if (g_progress) g_progress(std::atof(line.substr(0, tab).c_str()), text);
  } catch (...) {
  }
}

void finish(Ask& ask, const std::string& text) {
  if (!g_result_file.empty()) {
    std::ofstream f(P(g_result_file), std::ios::binary);
    f << text;
  }
  ask.info(text);
}

// Before anything is changed: the folder can be written, or (Windows) the installer runs again
// elevated with these answers, and its result is shown here. Returns -1 to go on here, else the
// status to end with.
int with_permission(Ask& ask, const fs::path& game, std::vector<std::string> answered) {
  // X360UI_TEST_NEEDS_PERMISSION: a test of the elevated run's way (Windows' CI has no UAC to answer).
  if (can_write(game) && !std::getenv("X360UI_TEST_NEEDS_PERMISSION")) return -1;
#ifdef _WIN32
  if (is_elevated() && !std::getenv("X360UI_TEST_ELEVATE_AS_USER"))
    throw Exit("Cannot write into " + S(game) + ", even as an administrator; nothing was changed.");
  std::cout << "the game's folder needs an administrator's permission: asking for it\n";
  std::error_code ec;
  const fs::path result = fs::temp_directory_path(ec) / ("x360ui-result-" + std::to_string(GetCurrentProcessId()) + ".txt");
  fs::remove(result, ec);
  answered.insert(answered.end(), {"--result-file", S(result)});
  const fs::path progress_file = fs::temp_directory_path(ec) / ("x360ui-progress-" + std::to_string(GetCurrentProcessId()) + ".txt");
  fs::remove(progress_file, ec);
  answered.insert(answered.end(), {"--progress-file", S(progress_file)});
  const int code = run_elevated(answered, [&] { forward_progress(progress_file); });
  fs::remove(progress_file, ec);
  fs::remove(S(progress_file) + ".new", ec);
  if (code < 0) throw Exit("No administrator's permission was given; nothing was changed.");
  std::string text;
  if (is_file(result)) {
    const Bytes b = read_file(result);
    text.assign(b.begin(), b.end());
    fs::remove(result, ec);
  }
  if (text.empty()) text = code == 0 ? "Done." : "The installer, run as an administrator, ended with status " + std::to_string(code) + ".";
  if (code == 0) ask.info(text);
  else ask.error(text);
  return code;
#else
  (void)ask;
  (void)answered;
  throw Exit("Cannot write into " + S(game) + " (no permission); nothing was changed.");
#endif
}

// ---- the proxy to install (install.proxy_dll: given, or the release's, here built in) ----
fs::path proxy_file(const std::string& build, const std::string& given, const fs::path& tmp) {
  if (!given.empty()) return P(given);
  const unsigned char* data = build == "steam" ? kProxySteam : kProxyGog;
  const size_t size = build == "steam" ? kProxySteamSize : kProxyGogSize;
  if (size) {
    const fs::path p = tmp / "CEGUIBase.dll";
    write_file(p, Bytes(data, data + size));
    return p;
  }
  throw Exit("This installer has no proxy for the " + build + " build; give one with --dll.");
}

struct Args {
  std::string command = "install", game, package, x360_dir, bag, dll, result_file, progress_file;
  bool yes = false, console = false, experimental = false, anyway = false, elevated = false;
  // Given to the elevated run by the first one: the save folders it found (that account's, not the
  // administrator's) and the answer that no backup is fine.
  std::vector<std::string> saves;
  bool saves_known = false, no_backup_ok = false;
};

Args parse(const std::vector<std::string>& argv) {
  Args a;
  size_t i = 0;
  if (!argv.empty() && (argv[0] == "install" || argv[0] == "uninstall" || argv[0] == "set-bag" || argv[0] == "list" || argv[0] == "notices"))
    a.command = argv[i++];
  for (; i < argv.size(); i++) {
    const std::string& s = argv[i];
    if (s == "--yes") a.yes = true;
    else if (s == "--console") a.console = true;
    else if (s == "--experimental") a.experimental = true;
    else if (s == "--anyway") a.anyway = true;
    else if (s == "--elevated") a.elevated = true;
    else if (s == "--saves-known") a.saves_known = true;
    else if (s == "--no-backup-ok") a.no_backup_ok = true;
    else if (i + 1 < argv.size() && s == "--save-folder") a.saves.push_back(argv[++i]);
    else if (i + 1 < argv.size() && s == "--result-file") a.result_file = argv[++i];
    else if (i + 1 < argv.size() && s == "--progress-file") a.progress_file = argv[++i];
    else if (i + 1 < argv.size() && (s == "--game" || s == "--package" || s == "--x360-dir" || s == "--bag" || s == "--dll")) {
      const std::string& v = argv[++i];
      (s == "--game" ? a.game : s == "--package" ? a.package : s == "--x360-dir" ? a.x360_dir : s == "--bag" ? a.bag : a.dll) = v;
    } else {
      throw Exit(kUsage);
    }
  }
  if (!a.bag.empty() && a.bag != "pc" && a.bag != "360") throw Exit("--bag is pc or 360");
  return a;
}

bool has_mod(const std::string& game) { return is_file(P(game) / "x360_ui.ini") || is_file(P(game) / "CEGUIBase_orig.dll"); }

std::pair<fs::path, std::string> pick_game(const std::string& given, Ask& ask, bool yes, bool installed_only = false) {
  if (!given.empty()) {
    std::error_code ec;
    const fs::path g = fs::weakly_canonical(fs::absolute(P(given)), ec);
    if (!is_game(S(g))) throw Exit("No Torchlight.exe and CEGUIBase.dll in " + S(g) + ".");
    return {g, build_of(S(g))};
  }
  const auto games = find_games(home_dir());
  for (const auto& [store, path] : games)
    if (is_experimental(build_of(path)))
      std::cout << store << ": " << path << ": its build is experimental (not yet played with the mod); give it with --game to install there.\n";
  for (const auto& [store, path] : games)
    if (build_of(path) == "unknown") std::cout << store << ": " << path << ": a build this version does not support yet, left alone.\n";
  std::vector<std::pair<std::string, std::string>> supported;
  for (const auto& [store, path] : games)
    if (build_of(path) != "unknown" && !is_experimental(build_of(path))) supported.push_back({store, path});
  if (supported.empty()) throw Exit("No supported PC Torchlight found; give its folder with --game.");
  // The owner: Steam's first, the others in the order found (install.py's sort, stable).
  std::stable_partition(supported.begin(), supported.end(), [](const auto& g) { return g.first == "steam"; });
  if (installed_only && std::any_of(supported.begin(), supported.end(), [](const auto& g) { return has_mod(g.second); })) {
    std::vector<std::pair<std::string, std::string>> with_mod;   // uninstall: only those with the mod
    for (const auto& g : supported) if (has_mod(g.second)) with_mod.push_back(g);
    supported = with_mod;
  }
  if (supported.size() > 1) {
    // More than one: each shown, and the player says which (--yes: the first, said so).
    std::string found = std::to_string(supported.size()) + " supported PC Torchlights were found:";
    for (const auto& [store, path] : supported) found += "\n" + store + ": " + path;
    if (yes) {
      std::cout << found << "\nusing " << supported[0].first << ": " << supported[0].second << " (--game picks another)\n";
      return {P(supported[0].second), build_of(supported[0].second)};
    }
    for (size_t i = 0; i < supported.size(); i++)
      if (ask.yes_no(found + "\n\nUse " + supported[i].first + ": " + supported[i].second + "?", i == 0))
        return {P(supported[i].second), build_of(supported[i].second)};
    throw Exit("No game chosen; give its folder with --game.");
  }
  return {P(supported[0].second), build_of(supported[0].second)};
}

int do_install(const Args& args) {
  Ask ask(args.console, args.yes);
  auto [game, build] = pick_game(args.game, ask, args.yes);
  if (build == "unknown") throw Exit(S(game) + ": a build this version does not support yet.");
  std::cout << "the game: " << S(game) << "\n";
  check_real_dll(S(game));
  bool experimental_ok = args.experimental;
  if (is_experimental(build) && !experimental_ok) {
    experimental_ok = Ask(args.console, false).yes_no("This game's build (" + build + ") is experimental: the mod was ported to it offline and has not been played on it yet. Install anyway?", false);
    if (!experimental_ok) {
      std::cout << "not installed (an experimental build needs a yes, or --experimental)\n";
      return 1;
    }
  }
  std::string source = !args.package.empty() ? args.package : !args.x360_dir.empty() ? args.x360_dir
                                                             : ask.file("Your Torchlight Xbox LIVE Arcade package (or a file in a folder extracted from it)");
  if (source.empty()) throw Exit("No 360 package given (--package FILE or --x360-dir DIR).");
  std::string bag = !args.bag.empty() ? args.bag : ask.yes_no(kBagWarning, false) ? "360" : "pc";
  std::vector<fs::path> folders;
  bool no_backup_ok = false;
  if (bag == "360") {
    if (args.saves_known) for (const auto& f : args.saves) folders.push_back(P(f));
    else folders = save_folders(game);
    no_backup_ok = folders.empty() && (args.no_backup_ok || ask.yes_no("No save folder of this game was found to back up (a Steam install's saves are in "
                                       "Steam's prefix, left alone; Heroic's prefix may be elsewhere). Back up your saves "
                                       "yourself first.\n\nUse the 360's bag without a backup?", false));
    if (folders.empty() && !no_backup_ok) {
      bag = "pc";
      std::cout << "PC's bag kept\n";
    }
  }
  // The proxy there is to install, before the slow part.
  if (!args.dll.empty()) {
    if (!is_file(P(args.dll))) throw Exit(args.dll + " does not exist.");
    if (!is_proxy(args.dll)) throw Exit(args.dll + " is not the proxy; not installing it");
  } else if (!(build == "steam" ? kProxySteamSize : kProxyGogSize)) {
    throw Exit("This installer has no proxy for the " + build + " build; give one with --dll.");
  }
  // Every answer is in: the permission, before anything is changed.
  {
    auto absolute = [](const std::string& p) {
      std::error_code ec;
      const fs::path a = fs::absolute(P(p), ec);
      if (ec) throw Exit("Cannot find the full path of " + p + ": " + ec.message());
      return S(a);
    };
    std::vector<std::string> again = {"install", "--game", S(game), "--package", absolute(source), "--bag", bag,
                                      "--yes", "--console", "--elevated", "--saves-known"};
    for (const auto& f : folders) again.insert(again.end(), {"--save-folder", S(f)});
    if (no_backup_ok) again.push_back("--no-backup-ok");
    if (experimental_ok) again.push_back("--experimental");
    if (!args.dll.empty()) again.insert(again.end(), {"--dll", absolute(args.dll)});
    if (!args.elevated) {
      const int code = with_permission(ask, game, again);
      if (code >= 0) return code;
    } else if (!can_write(game)) {
      throw Exit("Cannot write into " + S(game) + "; nothing was changed.");
    }
  }
  // The work folder in the game's folder (the paks are large for a /tmp in memory).
  const fs::path work = game / ("x360ui-work-" + std::to_string(std::time(nullptr)));
  fs::create_directories(work);
  struct Cleanup {
    fs::path p;
    ~Cleanup() { std::error_code ec; fs::remove_all(p, ec); }
  } cleanup{work};
  progress(0.02, "Reading the 360 copy");
  const auto [pak, image] = read_360_copy(source, work);
  std::cout << "the 360 copy: " << source << "\n";
  std::vector<fs::path> backups;
  if (bag == "360") {
    progress(0.15, "Backing up the saves");
    backups = back_up_saves(folders);
    for (const auto& b : backups) std::cout << "saves backed up: " << S(b) << "\n";
  }
  // The six interfaces converted into .partial folders: nothing in place is touched yet.
  const auto dests = converted_folders(game);
  // The .partial folders removed however this ends (once in place, there is none left to remove).
  struct Partials {
    std::vector<fs::path> paths;
    ~Partials() { for (const auto& p : paths) { std::error_code ec; fs::remove_all(p, ec); } }
  } guard;
  std::vector<fs::path>& partials = guard.paths;
  for (size_t k = 0; k < dests.size(); k++) {
    const fs::path partial = S(dests[k]) + ".partial";
    std::error_code ec;
    fs::remove_all(partial, ec);
    partials.push_back(partial);
    ConvertOptions o;
    o.pak = S(pak);
    o.out = S(partial);
    o.image = S(image);
    o.pc_pak = S(game / "Pak.zip");
    const std::string style = k == 0 ? "xbox" : kStyles[k - 1];
    o.style = style == "nintendo-swapped" ? "nintendo" : style;
    o.swap_confirm_cancel = style == "nintendo-swapped";
    std::cout << "converting the 360's interface (" << style << ")\n";
    progress(0.2 + 0.75 * double(k) / double(dests.size()), "Converting the 360's interface (" + style + ")");
    convert_ui(o);
  }
  const fs::path proxy = proxy_file(build, args.dll, work);
  // One step: the old folders aside, the new in place, the ini, the proxy; undone if any fails.
  progress(0.95, "Putting the mod in place");
  Change ch(game);
  try {
    for (size_t k = 0; k < dests.size(); k++) {
      ch.put_aside(dests[k]);
      ch.rename(partials[k], dests[k]);
    }
    ch.write(game / "x360_ui.ini", ini_text(game, dests[0], bag));
    install_proxy(ch, game, proxy);
    if (std::getenv("X360UI_TEST_FAIL_AFTER_PROXY")) throw Exit("a failure asked for by a test");
    ch.commit();
  } catch (...) {
    ch.rollback();
    throw;
  }
  progress(1.0, "Installed");
  std::string done = "Installed. Start Torchlight with a pad connected.";
  if (!backups.empty()) {
    done += "\n\nYour saves were backed up:";
    for (const auto& b : backups) done += "\n" + S(b);
  }
  finish(ask, done);
  return 0;
}

// The bag changed on an installed game, without the 360's package: the interface converted is the
// same for both bags (only x360_ui.ini's bag= differs), so only the ini is written; for the 360's
// bag the saves are backed up first, as install does (the owner, 2026-10-25: changing the bag asked
// for the package again).
int do_set_bag(const Args& args) {
  Ask ask(args.console, args.yes);
  auto [game, build] = pick_game(args.game, ask, args.yes, true);
  (void)build;
  if (args.bag != "pc" && args.bag != "360") throw Exit("set-bag needs --bag pc or --bag 360.");
  const std::string bag = args.bag;
  std::vector<fs::path> folders;
  bool no_backup_ok = false;
  if (bag == "360") {
    if (args.saves_known) for (const auto& f : args.saves) folders.push_back(P(f));
    else folders = save_folders(game);
    no_backup_ok = folders.empty() && (args.no_backup_ok || ask.yes_no("No save folder of this game was found to back up (a Steam install's saves are in "
                                       "Steam's prefix, left alone; Heroic's prefix may be elsewhere). Back up your saves "
                                       "yourself first.\n\nUse the 360's bag without a backup?", false));
    if (folders.empty() && !no_backup_ok) throw Exit("PC's bag kept: no save folder to back up was found.");
  }
  std::vector<std::string> again = {"set-bag", "--game", S(game), "--bag", bag, "--yes", "--console", "--elevated", "--saves-known"};
  for (const auto& f : folders) again.insert(again.end(), {"--save-folder", S(f)});
  if (no_backup_ok) again.push_back("--no-backup-ok");
  if (!args.elevated) {
    const int code = with_permission(ask, game, again);
    if (code >= 0) return code;
  } else if (!can_write(game)) {
    throw Exit("Cannot write into " + S(game) + "; nothing was changed.");
  }
  std::vector<fs::path> backups;
  if (bag == "360") {
    progress(0.3, "Backing up the saves");
    backups = back_up_saves(folders);
    for (const auto& b : backups) std::cout << "saves backed up: " << S(b) << "\n";
  }
  Change ch(game);
  ch.write(game / "x360_ui.ini", ini_text(game, converted_folders(game)[0], bag));
  ch.commit();
  progress(1.0, bag == "360" ? "The 360's bag installed" : "PC's bag installed");
  std::string done = bag == "360" ? "The 360's bag installed. Start Torchlight with a pad connected."
                                  : "PC's bag installed. Load and save each character before uninstalling.";
  if (!backups.empty()) {
    done += "\n\nYour saves were backed up:";
    for (const auto& b : backups) done += "\n" + S(b);
  }
  finish(ask, done);
  return 0;
}

int do_uninstall(const Args& args) {
  Ask ask(args.console, args.yes);
  auto [game, build] = pick_game(args.game, ask, args.yes, true);
  (void)build;
  bool backup = false;
  if (read_ini(game)["bag"] == "360") {
    if (!args.anyway && ask.yes_no("The 360's bag is installed. " + kNoModWarning +
                                       "\n\nSwitch to PC's bag now (recommended)? Then load and save each character, and uninstall.",
                                   true)) {
      if (!args.elevated) {
        const int code = with_permission(ask, game, {"uninstall", "--game", S(game), "--yes", "--console", "--elevated"});
        if (code >= 0) return code;
      }
      Change ch(game);
      ch.write(game / "x360_ui.ini", ini_text(game, converted_folders(game)[0], "pc"));
      ch.commit();
      progress(1.0, "PC's bag installed");
      finish(ask, "PC's bag installed; nothing uninstalled. Load and save each character, then uninstall.");
      return 0;
    }
    if (!args.anyway && !ask.yes_no("Uninstall anyway? The original game will delete the items that do not fit PC's bag. "
                                    "Your saves are backed up first.", false)) {
      std::cout << "not uninstalled\n";
      return 1;
    }
    backup = true;
  } else if (read_ini(game)["bag_360_used"] == "1") {
    if (!args.anyway && !ask.yes_no("PC's bag is installed, and the 360's was used before. " + kNoModWarningAfter +
                                        "\n\nHave you loaded and saved each character with PC's bag? Your saves are backed up first.",
                                    false)) {
      std::cout << "not uninstalled (load and save each character with PC's bag first)\n";
      return 1;
    }
    backup = true;
  }
  std::vector<fs::path> folders;
  if (backup) {
    if (args.saves_known) for (const auto& f : args.saves) folders.push_back(P(f));
    else folders = save_folders(game);
    if (folders.empty() && !args.saves_known) {   // a Steam install under Proton: its saves are in Steam's prefix, left alone
      if (args.anyway) {
        std::cout << "no save folder of this game was found: none backed up\n";
      } else if (!ask.yes_no(kNoSavesFound + "\n\nUninstall without a backup?", false)) {
        std::cout << "not uninstalled (back up your saves first)\n";
        return 1;
      }
    }
  }
  if (!args.elevated) {
    std::vector<std::string> again = {"uninstall", "--game", S(game), "--yes", "--console", "--elevated"};
    if (backup) {
      again.insert(again.end(), {"--anyway", "--saves-known"});
      for (const auto& f : folders) again.insert(again.end(), {"--save-folder", S(f)});
    }
    const int code = with_permission(ask, game, again);
    if (code >= 0) return code;
  }
  std::vector<fs::path> backups;
  progress(0.1, backup ? "Backing up the saves" : "Removing the mod");
  if (backup) {
    backups = back_up_saves(folders);
    for (const auto& b : backups) std::cout << "saves backed up: " << S(b) << "\n";
  }
  Change ch(game);
  try {
    if (!remove_proxy(ch, game)) throw Exit("CEGUIBase.dll is not the proxy nor the real one; nothing removed");
    std::vector<fs::path> gone, leftovers;
    for (const fs::path& f : converted_folders(game)) { gone.push_back(f); gone.push_back(S(f) + ".partial"); }
    std::error_code ec;
    for (fs::directory_iterator it(game, ec), end; !ec && it != end; it.increment(ec)) {
      const std::string n = S(it->path().filename());
      if (n.rfind("x360ui-undo-", 0) == 0) {   // a commit's leftover (a file in use): removed after, as it can be
        if (it->path() != ch.undo_folder() && !path_exists(it->path() / kKeepMark)) leftovers.push_back(it->path());
        continue;
      }
      if (n.rfind("x360ui-build-", 0) == 0 || n.rfind("x360ui-work-", 0) == 0 ||
          (n.rfind("x360_dump_", 0) == 0 && n.size() > 5 && n.compare(n.size() - 5, 5, ".json") == 0))
        gone.push_back(it->path());
    }
    for (const char* n : {"x360_ui.ini", "x360_ui.log", "x360_update.txt", "x360_settings.ini", "x360_stop.txt"}) gone.push_back(game / n);
    for (const fs::path& p : gone) ch.put_aside(p);
    if (std::getenv("X360UI_TEST_FAIL_AFTER_PROXY")) throw Exit("a failure asked for by a test");
    ch.commit();
    for (const fs::path& p : leftovers) fs::remove_all(p, ec);
    // The proxy's own folder, %LOCALAPPDATA%\x360ui (x360_stop.txt): beside each save folder's
    // AppData (under Wine, the prefix's), and Windows' own (install.mod_data_folders).
    std::vector<fs::path> data;
    for (const fs::path& f : save_folders(game)) data.push_back(f.parent_path().parent_path().parent_path().parent_path() / "Local" / "x360ui");
    {
      // Steam under Proton: the game in steamapps/common, its prefix in steamapps/compatdata/41500
      // (save_folders leaves Steam's prefix alone; only the mod's own folder is removed there).
      const fs::path common = fs::absolute(game, ec).parent_path();
      if (!ec && common.filename() == "common" && common.parent_path().filename() == "steamapps")
        data.push_back(common.parent_path() / "compatdata" / "41500" / "pfx" / "drive_c" / "users" / "steamuser" /
                       "AppData" / "Local" / "x360ui");
    }
#ifdef _WIN32
    if (const char* l = std::getenv("LOCALAPPDATA")) {
      data.push_back(fs::path(P(l)) / "x360ui");
      // Files an older version wrote beside the game in Program Files, which Windows redirected
      // into the user's VirtualStore (install.py the same).
      const fs::path store = fs::path(P(l)) / "VirtualStore" / fs::absolute(game, ec).relative_path();
      for (const char* n : {"x360_ui.log", "x360_settings.ini", "x360_stop.txt"}) fs::remove(store / n, ec);
    }
#endif
    for (const fs::path& d : data) if (is_dir(d)) fs::remove_all(d, ec);
  } catch (...) {
    ch.rollback();
    throw;
  }
  progress(1.0, "Uninstalled");
  std::string done = "Uninstalled from " + S(game) + ".";
  std::error_code ec;
  for (fs::directory_iterator it(game, ec), end; !ec && it != end; it.increment(ec))
    if (path_exists(it->path() / kKeepMark)) done += "\n\nKept, with originals to put back by hand: " + S(it->path());
  if (!backups.empty()) {
    done += "\n\nYour saves were backed up:";
    for (const auto& b : backups) done += "\n" + S(b);
  }
  finish(ask, done);
  return 0;
}

// The licence and the third-party notices this executable carries (miniz's MIT notice among them).
int do_notices() {
#ifdef _WIN32
  // The texts are UTF-8 (the manifest's code page is not the console's); the console's own is
  // given back after, as it outlives this process.
  const UINT code_page = GetConsoleOutputCP();
  SetConsoleOutputCP(65001);
#endif
  std::cout << "x360ui-install " << X360UI_VERSION << " is free software under the GNU General Public License 3.0\n"
               "(below); its source is https://github.com/YaPeL/torchlight-xbox-ui-pc at the tag " << X360UI_VERSION << ".\n\n"
               "==== Third-party code in this executable ====\n\n"
            << kNoticesText << "\n" << kSdlNoticesText << "\n==== GNU Lesser General Public License 2.1 (libmspack's LZX decoder) ====\n\n"
            << kLgplText << "\n==== SIL Open Font License 1.1 (Noto Sans, the wizard's font) ====\n\n" << kOflText
            << "\n==== GNU General Public License 3.0 ====\n\n" << kLicenseText;
#ifdef _WIN32
  std::cout.flush();
  if (code_page) SetConsoleOutputCP(code_page);
#endif
  return 0;
}

int do_list() {
  const auto games = find_games(home_dir());
  for (const auto& [store, path] : games) std::cout << store << ": " << path << (build_of(path) == "unknown" ? " (not supported by this version)" : "") << "\n";
  if (games.empty()) std::cout << "No PC Torchlight found.\n";
  return games.empty() ? 1 : 0;
}

}  // namespace

bool is_x360_pak(const fs::path& path);

namespace {
// An error, where the user sees it: for the first run that asked for elevation (the result file),
// else on the console and, without --console or --yes, in a window.
void report_error(const std::string& text, const std::vector<std::string>& argv) {
  if (!g_result_file.empty()) {
    std::cerr << text << "\n";
    std::ofstream f(P(g_result_file), std::ios::binary);
    f << text;
    return;
  }
  const bool console = std::find(argv.begin(), argv.end(), "--console") != argv.end();
  const bool yes = std::find(argv.begin(), argv.end(), "--yes") != argv.end();
  const bool list = !argv.empty() && (argv[0] == "list" || argv[0] == "notices");
  Ask(console || list, yes).error(text);
}
}  // namespace

namespace {
bool g_shared_console = false;
}  // namespace

void set_shared_console(bool shared) { g_shared_console = shared; }

int installer_main(const std::vector<std::string>& argv) {
  try {
    const Args a = parse(argv);
    if (g_shared_console && !a.yes && (a.command == "install" || a.command == "uninstall" || a.command == "set-bag"))
      throw Exit("Its questions would be asked in the terminal, whose input the shell reads too: give --yes with the "
                 "options (--game, --package or --x360-dir, --bag), or run it without arguments for its window.");
    g_result_file = a.result_file;
    g_progress_file = a.progress_file;
    if (a.command == "list") return do_list();
    if (a.command == "notices") return do_notices();
    if (a.command == "uninstall") return do_uninstall(a);
    if (a.command == "set-bag") return do_set_bag(a);
    return do_install(a);
  } catch (const Exit& e) {
    report_error(e.what() + (g_notes.empty() ? "" : "\n\nNot all could be undone:" + g_notes), argv);
    return 1;
  } catch (const std::exception& e) {
    report_error(std::string("error: ") + e.what() +
                     (g_notes.empty() ? "\nnothing was left half done (the game's folder is as it was)."
                                      : "\n\nNot all could be undone:" + g_notes),
                 argv);
    return 1;
  }
}

// ---- what the wizard asks (install.h) ----

void set_progress(Progress p) { g_progress = std::move(p); }

std::string warning_text(const std::string& which) {
  if (which == "bags") return kBags;
  if (which == "no-mod") return kNoModWarning;
  if (which == "no-mod-after") return kNoModWarningAfter;
  if (which == "no-saves") return kNoSavesFound;
  return "";
}

namespace {
GameFound describe(const std::string& store, const fs::path& game) {
  GameFound g;
  g.store = store;
  g.path = S(game);
  const std::string build = build_of(g.path);
  g.supported = build != "unknown" && !is_experimental(build);
  g.has_mod = has_mod(g.path);
  if (g.has_mod) {
    auto ini = read_ini(game);
    g.bag = ini["bag"] == "360" ? "360" : "pc";
    g.bag_360_used = ini["bag"] == "360" || ini["bag_360_used"] == "1";
  }
  return g;
}
}  // namespace

std::vector<GameFound> games_found() {
  std::vector<GameFound> out;
  for (const auto& [store, path] : find_games(home_dir())) out.push_back(describe(store, P(path)));
  std::stable_partition(out.begin(), out.end(), [](const GameFound& g) { return g.store == "steam"; });
  return out;
}

GameFound game_in(const std::string& chosen) {
  std::error_code ec;
  fs::path g = fs::weakly_canonical(fs::absolute(P(chosen), ec), ec);
  if (!is_dir(g)) g = g.parent_path();
  if (!is_game(S(g))) return {};
  return describe("", g);
}

std::string package_problem(const std::string& given) {
  std::string source = given;
  if (is_file(P(source))) {
    std::ifstream f(P(source), std::ios::binary);
    char magic[4] = {};
    f.read(magic, 4);
    const std::string m(magic, size_t(f.gcount()));
    if (m != "CON " && m != "LIVE" && m != "PIRS") source = S(P(source).parent_path());   // a file of an extracted folder
  }
  if (is_file(P(source))) {
    try {
      StfsPackage pkg(source);
      const std::string problem = check_package(pkg);
      if (!problem.empty()) return problem;
      for (const char* name : {"pak.zip", "default.xex"})
        if (!pkg.find(name)) return std::string("The package has no ") + name + ".";
    } catch (const Error& e) {
      return "Cannot read " + source + " as an Xbox 360 package: " + e.what();
    }
    return "";
  }
  if (!is_dir(P(source))) return source + " does not exist.";
  // What read_360_copy takes: the first pak.zip found, which must be the 360's.
  auto found = files_of_360_folder(P(source));
  if (!found.count("pak.zip")) return "No pak.zip in " + source + ".";
  if (!found.count("default.bin") && !found.count("default.xex")) return "No default.xex in " + source + ".";
  bool x360 = false;
  try {
    x360 = zip_has_name(S(found["pak.zip"]), kX360Layout);
  } catch (const std::exception&) {
  }
  if (!x360) return "The pak.zip found is not the Xbox 360 game's.";
  return "";
}

std::vector<std::string> saves_to_back_up(const std::string& game) {
  std::vector<std::string> out;
  for (const fs::path& f : save_folders(P(game))) out.push_back(S(f));
  return out;
}

bool needs_permission(const std::string& game) {
  return !can_write(P(game)) || std::getenv("X360UI_TEST_NEEDS_PERMISSION") != nullptr;
}

namespace {
bool is_x360_pak(const fs::path& path) {
  try {
    return zip_has_name(S(path), kX360Layout);
  } catch (const std::exception&) {
    return false;
  }
}
}  // namespace

}  // namespace x360ui
