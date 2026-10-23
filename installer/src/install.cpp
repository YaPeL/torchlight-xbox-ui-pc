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

#include "bytes.h"
#include "convert.h"
#include "game.h"
#include "paths.h"
#include "stfs.h"
#include "xex.h"

#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
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

namespace {

const char* const kUsage =
    "x360ui-install [install] [--package FILE | --x360-dir DIR] [--game DIR] [--bag pc|360] [--yes] [--console]\n"
    "               [--experimental] [--dll FILE]\n"
    "x360ui-install uninstall [--game DIR] [--yes] [--console] [--anyway]\n"
    "x360ui-install list\n";
const char* const kStyles[] = {"playstation3", "playstation4", "playstation5", "nintendo", "nintendo-swapped"};
const uint32_t kX360Title = 0x58410A7E, kX360ContentType = 0x000D0000;
const char* const kX360Layout = "media/ui/mastermenu_xenon.layout";
const std::string kNoModWarning =
    "Without the mod (uninstalled, or removed by Steam or GOG when they verify, repair or update the "
    "game) the original game deletes the items that do not fit PC's bag. Before uninstalling, install "
    "PC's bag, then load and save each character.";
const std::string kBagWarning =
    "The 360's bag has one section of 50 slots for every kind of item; PC's has three of 21 (any "
    "item, spells, fish).\n\n"
    "When a character changes bag, the mod moves the items that do not fit to free slots or to the "
    "pet's bag; if some still do not fit, the game says so and closes without saving.\n\n" +
    kNoModWarning + "\n\nThe saves found are backed up first.\n\nUse the 360's bag?";
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

// ---- questions: in windows when there is a display, else on the console (Python's Ask) ----
class Ask {
 public:
  Ask(bool console, bool yes) : yes_(yes), gui_(!console && !yes && gui_available()) {}
  std::string file(const std::string& title) {
    if (yes_) return "";
    if (gui_) return gui_file(title);
    std::cout << title << ": " << std::flush;
    std::string line;
    if (!std::getline(std::cin, line)) return "";
    trim(line);
    if (line.size() >= 2 && line.front() == '"' && line.back() == '"') line = line.substr(1, line.size() - 2);
    return line;
  }
  bool yes_no(const std::string& text, bool def) {
    if (yes_) return def;
    if (gui_) return gui_yes_no(text);
    std::cout << text << (def ? " [Y/n] " : " [y/N] ") << std::flush;
    std::string a;
    if (!std::getline(std::cin, a)) return def;
    trim(a);
    if (a.empty()) return def;
    return a[0] == 'y' || a[0] == 'Y';
  }
  void info(const std::string& text) {
    std::cout << text << "\n";
    if (gui_) gui_info(text);
  }

 private:
  bool yes_, gui_;
  static void trim(std::string& s) {
    while (!s.empty() && std::isspace((unsigned char)s.back())) s.pop_back();
    size_t i = 0;
    while (i < s.size() && std::isspace((unsigned char)s[i])) i++;
    s = s.substr(i);
  }
#ifdef _WIN32
  static std::wstring w(const std::string& s) { return fs_path(s).wstring(); }
  static bool gui_available() { return true; }
  static bool gui_yes_no(const std::string& t) { return MessageBoxW(nullptr, w(t).c_str(), w(kTitle).c_str(), MB_YESNO | MB_ICONQUESTION) == IDYES; }
  static void gui_info(const std::string& t) { MessageBoxW(nullptr, w(t).c_str(), w(kTitle).c_str(), MB_OK | MB_ICONINFORMATION); }
  static std::string gui_file(const std::string& title) {
    wchar_t buf[4096] = L"";
    std::wstring wt = w(title);
    OPENFILENAMEW o{};
    o.lStructSize = sizeof o;
    o.lpstrFile = buf;
    o.nMaxFile = 4096;
    o.lpstrTitle = wt.c_str();
    o.Flags = OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    return GetOpenFileNameW(&o) ? S(fs::path(buf)) : "";
  }
#else
  // zenity (GNOME and most desktops) or kdialog (KDE), when there is a display.
  static std::string tool() {
    if (!std::getenv("DISPLAY") && !std::getenv("WAYLAND_DISPLAY")) return "";
    for (const char* t : {"zenity", "kdialog"})
      for (const char* dir : {"/usr/bin/", "/bin/", "/usr/local/bin/"})
        if (access((std::string(dir) + t).c_str(), X_OK) == 0) return std::string(dir) + t;
    return "";
  }
  static bool gui_available() { return !tool().empty(); }
  static int run(const std::vector<std::string>& argv, std::string* out) {
    int pipefd[2];
    if (pipe(pipefd) != 0) return -1;
    pid_t pid = fork();
    if (pid == 0) {
      dup2(pipefd[1], 1);
      close(pipefd[0]);
      std::vector<char*> a;
      for (const auto& s : argv) a.push_back(const_cast<char*>(s.c_str()));
      a.push_back(nullptr);
      execv(a[0], a.data());
      _exit(127);
    }
    close(pipefd[1]);
    char buf[4096];
    ssize_t n;
    while ((n = read(pipefd[0], buf, sizeof buf)) > 0)
      if (out) out->append(buf, size_t(n));
    close(pipefd[0]);
    int status = 0;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
  }
  static bool is_zenity() { return tool().find("zenity") != std::string::npos; }
  static bool gui_yes_no(const std::string& t) {
    return run(is_zenity() ? std::vector<std::string>{tool(), "--question", "--title", kTitle, "--text", t, "--width", "500"}
                           : std::vector<std::string>{tool(), "--title", kTitle, "--yesno", t},
               nullptr) == 0;
  }
  static void gui_info(const std::string& t) {
    run(is_zenity() ? std::vector<std::string>{tool(), "--info", "--title", kTitle, "--text", t, "--width", "500"}
                    : std::vector<std::string>{tool(), "--title", kTitle, "--msgbox", t},
        nullptr);
  }
  static std::string gui_file(const std::string& title) {
    std::string out;
    if (run(is_zenity() ? std::vector<std::string>{tool(), "--file-selection", "--title", title}
                        : std::vector<std::string>{tool(), "--title", title, "--getopenfilename", home_dir()},
            &out) != 0)
      return "";
    while (!out.empty() && (out.back() == '\n' || out.back() == '\r')) out.pop_back();
    return out;
  }
#endif
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
      try {
        std::ofstream out(dest, std::ios::binary);
        pkg->read_to(*e, out);
      } catch (const Error& err) {
        throw Exit(std::string("Cannot read ") + name + " from the package: " + err.what());
      }
    }
  } else if (is_dir(P(source))) {
    std::map<std::string, fs::path> found;
    // os.walk: each folder's files, then its subfolders, top down.
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
    walk(P(source));
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
    const fs::path copy = S(f) + "-backup-" + stamp;
    fs::copy(f, copy, fs::copy_options::recursive);
    copies.push_back(copy);
  }
  return copies;
}

// ---- the ini (install.read_ini, write_ini) ----
std::map<std::string, std::string> read_ini(const fs::path& game) {
  std::map<std::string, std::string> v;
  std::ifstream f(game / "x360_ui.ini", std::ios::binary);
  for (std::string line; std::getline(f, line);) {
    while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();
    const size_t eq = line.find('=');
    if (eq == std::string::npos) continue;
    auto strip = [](std::string s) {
      while (!s.empty() && std::isspace((unsigned char)s.back())) s.pop_back();
      size_t i = 0;
      while (i < s.size() && std::isspace((unsigned char)s[i])) i++;
      return s.substr(i);
    };
    v[strip(line.substr(0, eq))] = strip(line.substr(eq + 1));
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
  const std::vector<std::pair<std::string, std::string>> owned = {
      {"mode", "scheme"}, {"out", wine_path(out)}, {"pad", "1"}, {"pad_mode", "direct"}, {"prompts", "auto"}, {"bag", bag}};
  std::set<std::string> keys = {"mode", "out", "pad", "pad_mode", "prompts", "bag", "update_check"};
  std::vector<std::string> kept;
  std::ifstream f(game / "x360_ui.ini", std::ios::binary);
  for (std::string line; std::getline(f, line);) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    std::string key = line.substr(0, line.find('='));
    while (!key.empty() && std::isspace((unsigned char)key.back())) key.pop_back();
    size_t i = 0;
    while (i < key.size() && std::isspace((unsigned char)key[i])) i++;
    if (!keys.count(key.substr(i))) kept.push_back(line);
  }
  std::string t;
  for (const auto& [k, v] : owned) t += k + "=" + v + "\r\n";
  for (const std::string& l : kept) {
    bool blank = true;
    for (char c : l) blank = blank && std::isspace((unsigned char)c);
    if (!blank) t += l + "\r\n";
  }
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
    write_file(p, d);
    steps_.push_back({Step::Created, p, {}});
  }
  void copy(const fs::path& from, const fs::path& to) {
    put_aside(to);
    fs::copy_file(from, to);
    steps_.push_back({Step::Created, to, {}});
  }
  void commit() {
    done_ = true;
    std::error_code ec;
    fs::remove_all(undo_, ec);
  }
  void rollback() {
    done_ = true;
    for (auto it = steps_.rbegin(); it != steps_.rend(); ++it) {
      std::error_code ec;
      if (it->kind == Step::Created) fs::remove_all(it->a, ec);
      else if (it->kind == Step::Renamed) fs::rename(it->b, it->a, ec);
      else fs::rename(it->b, it->a, ec);
      if (ec) std::cerr << "could not undo a change to " << S(it->a) << ": " << ec.message() << "\n";
    }
    std::error_code ec;
    fs::remove_all(undo_, ec);
  }

 private:
  struct Step {
    enum Kind { Aside, Renamed, Created } kind;
    fs::path a, b;
  };
  static unsigned rand_id() { return unsigned(std::chrono::steady_clock::now().time_since_epoch().count() & 0xFFFFFF); }
  fs::path game_, undo_;
  std::vector<Step> steps_;
  bool done_ = false;
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
int run_elevated(const std::vector<std::string>& args) {
  if (std::getenv("X360UI_NO_ELEVATE")) return -1;
  wchar_t self[MAX_PATH];
  GetModuleFileNameW(nullptr, self, MAX_PATH);
  std::wstring params;
  for (const std::string& a : args) params += quote(a) + L" ";
  SHELLEXECUTEINFOW sei{};
  sei.cbSize = sizeof sei;
  sei.fMask = SEE_MASK_NOCLOSEPROCESS;
  sei.lpVerb = L"runas";
  sei.lpFile = self;
  sei.lpParameters = params.c_str();
  sei.nShow = SW_SHOWNORMAL;
  if (!ShellExecuteExW(&sei)) return -1;   // ERROR_CANCELLED: the user said no
  WaitForSingleObject(sei.hProcess, INFINITE);
  DWORD code = 1;
  GetExitCodeProcess(sei.hProcess, &code);
  CloseHandle(sei.hProcess);
  return int(code);
}
#endif

// Before anything is changed: the folder can be written, or (Windows) the installer runs again
// elevated with these answers. Returns -1 to go on here, else the status to end with.
int ensure_permission(const fs::path& game, const std::vector<std::string>& answered) {
  if (can_write(game)) return -1;
#ifdef _WIN32
  if (is_elevated()) throw Exit("Cannot write into " + S(game) + ", even as an administrator; nothing was changed.");
  std::cout << "the game's folder needs an administrator's permission: asking for it\n";
  const int code = run_elevated(answered);
  if (code < 0) throw Exit("No administrator's permission was given; nothing was changed.");
  return code;
#else
  (void)answered;
  throw Exit("Cannot write into " + S(game) + " (no permission); nothing was changed.");
#endif
}

// ---- the proxy to install (install.proxy_dll: given, the release's built in, or beside the executable) ----
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
  std::string command = "install", game, package, x360_dir, bag, dll;
  bool yes = false, console = false, experimental = false, anyway = false, elevated = false;
};

Args parse(const std::vector<std::string>& argv) {
  Args a;
  size_t i = 0;
  if (!argv.empty() && (argv[0] == "install" || argv[0] == "uninstall" || argv[0] == "list")) a.command = argv[i++];
  for (; i < argv.size(); i++) {
    const std::string& s = argv[i];
    if (s == "--yes") a.yes = true;
    else if (s == "--console") a.console = true;
    else if (s == "--experimental") a.experimental = true;
    else if (s == "--anyway") a.anyway = true;
    else if (s == "--elevated") a.elevated = true;
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

std::pair<fs::path, std::string> pick_game(const std::string& given) {
  if (!given.empty()) {
    std::error_code ec;
    const fs::path g = fs::weakly_canonical(fs::absolute(P(given)), ec);
    if (!is_game(S(g))) throw Exit("No Torchlight.exe and CEGUIBase.dll in " + S(g) + ".");
    return {g, build_of(S(g))};
  }
  const auto games = find_games(home_dir());
  for (const auto& [store, path] : games)
    if (is_experimental(build_of(path)))
      std::cout << store << ": " << path << ": its build is supported experimentally (not yet played with the mod); give it with --game to install there.\n";
  for (const auto& [store, path] : games)
    if (build_of(path) == "unknown") std::cout << store << ": " << path << ": a build this version does not support yet, left alone.\n";
  for (const auto& [store, path] : games)
    if (build_of(path) != "unknown" && !is_experimental(build_of(path))) return {P(path), build_of(path)};
  throw Exit("No supported PC Torchlight found; give its folder with --game.");
}

int do_install(const Args& args) {
  Ask ask(args.console, args.yes);
  auto [game, build] = pick_game(args.game);
  if (build == "unknown") throw Exit(S(game) + ": a build this version does not support yet.");
  std::cout << "the game: " << S(game) << " (" << build << " build)\n";
  check_real_dll(S(game));
  bool experimental_ok = args.experimental;
  if (is_experimental(build) && !experimental_ok) {
    experimental_ok = Ask(args.console, false).yes_no("This game's build (" + build + ") is supported experimentally: the mod was ported to it offline and has not been played on it yet. Install anyway?", false);
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
  if (bag == "360") {
    folders = save_folders(game);
    if (folders.empty() && !ask.yes_no("No save folder of this game was found to back up (a Steam install's saves are in "
                                       "Steam's prefix, left alone; Heroic's prefix may be elsewhere). Back up your saves "
                                       "yourself first.\n\nUse the 360's bag without a backup?", false)) {
      bag = "pc";
      std::cout << "PC's bag kept\n";
    }
  }
  // Every answer is in: the permission, before anything is changed.
  {
    std::vector<std::string> again = {"install", "--game", S(game), "--package", source, "--bag", bag, "--yes", "--console", "--elevated"};
    if (experimental_ok) again.push_back("--experimental");
    if (!args.dll.empty()) again.insert(again.end(), {"--dll", args.dll});
    if (!args.elevated) {
      const int code = ensure_permission(game, again);
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
  const auto [pak, image] = read_360_copy(source, work);
  std::cout << "the 360 copy: " << source << "\n";
  std::vector<fs::path> backups;
  if (bag == "360") {
    backups = back_up_saves(folders);
    for (const auto& b : backups) std::cout << "saves backed up: " << S(b) << "\n";
  }
  // The six interfaces converted into .partial folders: nothing in place is touched yet.
  const auto dests = converted_folders(game);
  std::vector<fs::path> partials;
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
    try {
      convert_ui(o);
    } catch (...) {
      for (const auto& p : partials) fs::remove_all(p, ec);
      throw;
    }
  }
  const fs::path proxy = proxy_file(build, args.dll, work);
  // One step: the old folders aside, the new in place, the ini, the proxy; undone if any fails.
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
    std::error_code ec;
    for (const auto& p : partials) fs::remove_all(p, ec);
    throw;
  }
  std::string done = "Installed. Start Torchlight with a pad connected.";
  if (!backups.empty()) {
    done += "\n\nYour saves were backed up:";
    for (const auto& b : backups) done += "\n" + S(b);
  }
  ask.info(done);
  return 0;
}

int do_uninstall(const Args& args) {
  Ask ask(args.console, args.yes);
  auto [game, build] = pick_game(args.game);
  (void)build;
  bool backup = false;
  if (read_ini(game)["bag"] == "360") {
    if (!args.anyway && ask.yes_no("The 360's bag is installed. " + kNoModWarning +
                                       "\n\nSwitch to PC's bag now (recommended)? Then load and save each character, and uninstall.",
                                   true)) {
      if (!args.elevated) {
        const int code = ensure_permission(game, {"uninstall", "--game", S(game), "--yes", "--console", "--elevated"});
        if (code >= 0) return code;
      }
      Change ch(game);
      ch.write(game / "x360_ui.ini", ini_text(game, converted_folders(game)[0], "pc"));
      ch.commit();
      ask.info("PC's bag installed; nothing uninstalled. Load and save each character, then uninstall.");
      return 0;
    }
    if (!args.anyway && !ask.yes_no("Uninstall anyway? The original game will delete the items that do not fit PC's bag. "
                                    "Your saves are backed up first.", false)) {
      std::cout << "not uninstalled\n";
      return 1;
    }
    backup = true;
  }
  if (!args.elevated) {
    std::vector<std::string> again = {"uninstall", "--game", S(game), "--yes", "--console", "--elevated"};
    if (backup) again.push_back("--anyway");
    const int code = ensure_permission(game, again);
    if (code >= 0) return code;
  }
  std::vector<fs::path> backups;
  if (backup) {
    backups = back_up_saves(save_folders(game));
    for (const auto& b : backups) std::cout << "saves backed up: " << S(b) << "\n";
  }
  Change ch(game);
  try {
    if (!remove_proxy(ch, game)) throw Exit("CEGUIBase.dll is not the proxy nor the real one; nothing removed");
    std::vector<fs::path> gone;
    for (const fs::path& f : converted_folders(game)) { gone.push_back(f); gone.push_back(S(f) + ".partial"); }
    std::error_code ec;
    for (fs::directory_iterator it(game, ec), end; !ec && it != end; it.increment(ec)) {
      const std::string n = S(it->path().filename());
      if (n.rfind("x360ui-build-", 0) == 0 || n.rfind("x360ui-work-", 0) == 0 ||
          (n.rfind("x360_dump_", 0) == 0 && n.size() > 5 && n.compare(n.size() - 5, 5, ".json") == 0))
        gone.push_back(it->path());
    }
    for (const char* n : {"x360_ui.ini", "x360_ui.log", "x360_update.txt", "x360_settings.ini", "x360_stop.txt"}) gone.push_back(game / n);
    for (const fs::path& p : gone) ch.put_aside(p);
    if (std::getenv("X360UI_TEST_FAIL_AFTER_PROXY")) throw Exit("a failure asked for by a test");
    ch.commit();
  } catch (...) {
    ch.rollback();
    throw;
  }
  std::string done = "Uninstalled from " + S(game) + ".";
  if (!backups.empty()) {
    done += "\n\nYour saves were backed up:";
    for (const auto& b : backups) done += "\n" + S(b);
  }
  ask.info(done);
  return 0;
}

int do_list() {
  const auto games = find_games(home_dir());
  for (const auto& [store, path] : games) std::cout << store << ": " << path << " (" << build_of(path) << " build)\n";
  if (games.empty()) std::cout << "No PC Torchlight found.\n";
  return games.empty() ? 1 : 0;
}

}  // namespace

bool is_x360_pak(const fs::path& path);

int installer_main(const std::vector<std::string>& argv) {
  try {
    const Args a = parse(argv);
    if (a.command == "list") return do_list();
    if (a.command == "uninstall") return do_uninstall(a);
    return do_install(a);
  } catch (const Exit& e) {
    std::cerr << e.what() << "\n";
    return 1;
  } catch (const std::exception& e) {
    std::cerr << "error: " << e.what() << "\nnothing was left half done (the game's folder is as it was).\n";
    return 1;
  }
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
