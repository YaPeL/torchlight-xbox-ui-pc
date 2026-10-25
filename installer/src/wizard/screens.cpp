// The wizard's screens (the owner's list, 2026-10-24): the games found, what to do with one that
// has the mod, the 360's package, the bag, the summary, the progress and the result; the
// system's pickers, and the wizard's own browser where there is none (the Steam Deck's Game Mode).
#include "wizard_screens.h"

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <sstream>
#include <thread>
#include <vector>

#include "install.h"
#include "paths.h"
#include "text_lines.h"

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

namespace fs = std::filesystem;

namespace x360ui::wizard {
namespace {

enum class Screen { Games, Action, Uninstall, Package, Bag, Summary, Working, Result, Browser };
enum class Goal { Install, Reinstall, ToPcBag, To360Bag, Uninstall, UninstallStep1, UninstallAnyway };

const char* screen_name(Screen s) {
  switch (s) {
    case Screen::Games: return "games";
    case Screen::Action: return "action";
    case Screen::Uninstall: return "uninstall";
    case Screen::Package: return "package";
    case Screen::Bag: return "bag";
    case Screen::Summary: return "summary";
    case Screen::Working: return "working";
    case Screen::Result: return "result";
    case Screen::Browser: return "browser";
  }
  return "";
}

std::string store_label(const std::string& store) {
  if (store == "steam") return "Steam";
  if (store == "epic") return "Epic Games Store";
  if (store == "gog") return "GOG";
  return "Folder chosen";
}

// A path the player chose, from the system's pickers (UTF-8) or the browser.
std::string utf8(const fs::path& p) { return utf8_path(p); }

struct Entry {
  std::string name;
  fs::path path;
  bool folder = false;
};

}  // namespace

struct Screens::State {
  SDL_Window* window;
  Fonts fonts;
  Options options;
  Screen screen = Screen::Games;
  std::vector<Screen> history;   // Back's way
  bool entered = true;           // the screen's first frames: its default item takes the focus
  int entered_frames = 0;
  bool closed = false;

  // The game.
  std::vector<GameFound> games;
  int chosen = -1;
  GameFound game;
  std::string game_problem;
  Goal goal = Goal::Install;
  // The 360's package.
  std::string package, package_problem_text;
  // The bag.
  std::string bag = "pc";
  std::vector<std::string> saves;
  bool no_backup_ok = false, loaded_each = false, anyway_ok = false;

  // The system's picker answers on its own thread.
  std::mutex pick_mutex;
  bool pick_pending = false, pick_answered = false, pick_failed = false;
  std::string pick_path;
  bool pick_for_game = false;

  // The wizard's own browser.
  bool browse_for_game = false;
  fs::path browse_dir;
  std::vector<Entry> entries;
  std::string browse_error;
  bool browse_drives = false;

  // The install, on its own thread, with its output kept.
  std::thread worker;
  std::atomic<bool> working{false}, worked{false};
  std::mutex progress_mutex;
  double fraction = 0;
  std::string progress_text;
  int code = 0;
  std::ostringstream out, err;
  std::streambuf *cout_buf = nullptr, *cerr_buf = nullptr;
  std::string result_text;
  std::vector<std::string> backups;

  State(SDL_Window* w, Fonts f, const Options& o) : window(w), fonts(f), options(o) { games = games_found(); }

  void go(Screen next) {
    history.push_back(screen);
    screen = next;
    entered = true;
  }
  void back() {
    if (history.empty()) return;
    screen = history.back();
    history.pop_back();
    entered = true;
  }
  void restart() {
    history.clear();
    screen = Screen::Games;
    entered = true;
    games = games_found();
    chosen = -1;
    package.clear();
    package_problem_text.clear();
    bag = "pc";
    no_backup_ok = loaded_each = anyway_ok = false;
  }

  // ---- the parts every screen has ----
  void heading(const char* title, const char* step) {
    ImGui::PushFont(fonts.bold, 30.0f);
    ImGui::TextUnformatted(kTitle);
    ImGui::PopFont();
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::TextUnformatted(step);
    ImGui::PopStyleColor();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::PushFont(fonts.bold, 24.0f);
    ImGui::TextUnformatted(title);
    ImGui::PopFont();
    ImGui::Spacing();
  }
  // The body's height: the window's, less the footer's.
  float body_height() const {
    const ImGuiStyle& st = ImGui::GetStyle();
    return ImGui::GetContentRegionAvail().y - (ImGui::GetFrameHeight() + st.ItemSpacing.y * 3 + ImGui::GetTextLineHeight());
  }
  void hint_line(bool next = false) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    const char* hint = next ? "A: choose     B: back     Start: next     D-pad or arrows: move"
                            : "A: choose     B: back     D-pad or arrows: move";
    ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize(hint).x) * 0.5f);
    ImGui::TextUnformatted(hint);
    ImGui::PopStyleColor();
  }
  // Back on the left, the next step on the right; true when the next was chosen.
  bool footer(const char* next_label, bool next_enabled, bool back_enabled = true, bool focus_next = false) {
    ImGui::Separator();
    if (!back_enabled) ImGui::BeginDisabled();
    if (ImGui::Button("Back", ImVec2(ImGui::GetFontSize() * 7, 0))) back();
    if (!back_enabled) ImGui::EndDisabled();
    bool chosen_next = false;
    if (next_label) {
      const float w = std::max(ImGui::GetFontSize() * 7, ImGui::CalcTextSize(next_label).x + ImGui::GetStyle().FramePadding.x * 2);
      ImGui::SameLine(ImGui::GetWindowWidth() - w - ImGui::GetStyle().WindowPadding.x);
      if (!next_enabled) ImGui::BeginDisabled();
      if (focus_next && entered && next_enabled) focus();
      chosen_next = ImGui::Button(next_label, ImVec2(w, 0));
      if (!next_enabled) ImGui::EndDisabled();
      // Start: the next step from anywhere on the screen.
      if (next_enabled && !entered && ImGui::IsKeyPressed(ImGuiKey_GamepadStart, false)) chosen_next = true;
    }
    hint_line(next_label != nullptr);
    return chosen_next;
  }
  void wrapped(const std::string& text) { ImGui::TextWrapped("%s", text.c_str()); }
  void muted(const std::string& text) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    wrapped(text);
    ImGui::PopStyleColor();
  }
  // A box in the warning's colour, its height its text's.
  void warning_box(const char* id, const std::string& title, const std::string& text) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.30f, 0.20f, 0.00f, 0.55f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.90f, 0.70f, 0.00f, 1.00f));
    ImGui::BeginChild(id, ImVec2(0, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysUseWindowPadding);
    ImGui::PushFont(fonts.bold, 0.0f);
    ImGui::TextColored(ImVec4(1.00f, 0.80f, 0.30f, 1.00f), "%s", title.c_str());
    ImGui::PopFont();
    wrapped(text);
    ImGui::EndChild();
    ImGui::PopStyleColor(2);
  }
  void error_text(const std::string& text) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.00f, 0.45f, 0.40f, 1.00f));
    wrapped(text);
    ImGui::PopStyleColor();
  }
  // The screen's default item: focused, with the pad's cursor shown (ImGui ignores a first A
  // while its cursor is hidden).
  static void focus(int offset = 0) {
    ImGui::SetKeyboardFocusHere(offset);
    ImGui::SetNavCursorVisible(true);
  }
  void focus_here_once() {
    if (entered) focus();
  }

  // ---- the system's pickers, else the wizard's browser ----
  static void picked(void* user, const char* const* files, int) {
    State* s = static_cast<State*>(user);
    std::lock_guard<std::mutex> lock(s->pick_mutex);
    s->pick_answered = true;
    s->pick_failed = !files;
    s->pick_path = files && files[0] ? files[0] : "";
  }
  void pick(bool for_game, bool folder) {
    pick_for_game = for_game;
    if (options.own_picker) {
      open_browser(for_game);
      return;
    }
    pick_pending = true;
    pick_answered = false;
    SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetPointerProperty(props, SDL_PROP_FILE_DIALOG_WINDOW_POINTER, window);
    SDL_SetStringProperty(props, SDL_PROP_FILE_DIALOG_TITLE_STRING,
                          for_game ? "The game's folder (Torchlight.exe is in it)" : "Your Torchlight Xbox LIVE Arcade package");
    SDL_ShowFileDialogWithProperties(folder ? SDL_FILEDIALOG_OPENFOLDER : SDL_FILEDIALOG_OPENFILE, picked, this, props);
    SDL_DestroyProperties(props);
  }
  void poll_pick() {
    std::string path;
    bool failed;
    {
      std::lock_guard<std::mutex> lock(pick_mutex);
      if (!pick_pending || !pick_answered) return;
      pick_pending = false;
      path = pick_path;
      failed = pick_failed;
    }
    if (failed) open_browser(pick_for_game);   // no picker here (no desktop portal)
    else if (!path.empty()) chose(pick_for_game, path);
  }
  void chose(bool for_game, const std::string& path) {
    if (for_game) {
      const GameFound g = game_in(path);
      if (g.path.empty()) {
        game_problem = "No Torchlight.exe and CEGUIBase.dll in " + path + ".";
        return;
      }
      game_problem.clear();
      auto same = std::find_if(games.begin(), games.end(), [&](const GameFound& f) { return f.path == g.path; });
      if (same == games.end()) {
        games.push_back(g);
        chosen = int(games.size()) - 1;
      } else {
        chosen = int(same - games.begin());
      }
    } else {
      package = path;
      package_problem_text = package_problem(path);
    }
  }
  void open_browser(bool for_game) {
    {
      std::lock_guard<std::mutex> lock(pick_mutex);
      pick_pending = false;   // a system picker that never answered: its late answer is ignored
    }
    browse_for_game = for_game;
    std::error_code ec;
    fs::path start = !options.browse_from.empty() ? fs_path(options.browse_from) : fs::path();
    if (start.empty() && !for_game && !package.empty()) start = fs_path(package).parent_path();
    if (start.empty()) {
      const char* home = std::getenv("HOME");
#ifdef _WIN32
      if (!home) home = std::getenv("USERPROFILE");
#endif
      start = home ? fs_path(home) : fs::current_path(ec);
    }
    browse_drives = false;
    list(start);
    go(Screen::Browser);
  }
  void list(const fs::path& dir) {
    entries.clear();
    browse_error.clear();
    browse_dir = dir;
    std::error_code ec;
    std::vector<Entry> folders, files;
    for (fs::directory_iterator it(dir, fs::directory_options::skip_permission_denied, ec), end; !ec && it != end; it.increment(ec)) {
      Entry e{utf8(it->path().filename()), it->path(), it->is_directory(ec)};
      (e.folder ? folders : files).push_back(e);
    }
    if (ec) browse_error = "Cannot read " + utf8(dir) + ": " + ec.message();
    auto by_name = [](const Entry& a, const Entry& b) { return a.name < b.name; };
    std::sort(folders.begin(), folders.end(), by_name);
    std::sort(files.begin(), files.end(), by_name);
    entries = folders;
    if (!browse_for_game) entries.insert(entries.end(), files.begin(), files.end());
  }
  void list_drives() {
    entries.clear();
    browse_drives = true;
#ifdef _WIN32
    const DWORD mask = GetLogicalDrives();
    for (int i = 0; i < 26; i++)
      if (mask & (1u << i)) {
        const std::string root = std::string(1, char('A' + i)) + ":\\";
        entries.push_back({root, fs_path(root), true});
      }
#endif
  }

  // ---- the screens ----
  void games_screen() {
    heading("Which Torchlight?", "1  Game   >   2  Package   >   3  Bag   >   4  Summary");
    ImGui::BeginChild("body", ImVec2(0, body_height()), ImGuiChildFlags_NavFlattened);
    if (games.empty()) muted("No PC Torchlight was found. Choose its folder below.");
    for (size_t i = 0; i < games.size(); i++) {
      const GameFound& g = games[i];
      ImGui::PushID(int(i));
      std::string state = !g.supported ? "A build this version does not support yet"
                          : !g.has_mod ? "The mod is not installed"
                          : g.bag == "360" ? "The mod is installed, with the 360's bag"
                                           : "The mod is installed, with PC's bag";
      if (!g.supported) ImGui::BeginDisabled();
      if (entered && ((chosen < 0 && g.supported) || chosen == int(i))) {
        focus();
        if (chosen < 0) chosen = int(i);
      }
      const float h = ImGui::GetTextLineHeightWithSpacing() * 2 + ImGui::GetStyle().FramePadding.y;
      const bool pressed = ImGui::Selectable("##game", chosen == int(i), 0, ImVec2(0, h));
      if (pressed) chosen = int(i);
      // A double click, or A or Enter on it: chosen and on to the next screen.
      if ((pressed && !ImGui::IsMouseReleased(0)) || (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))) next_from_games();
      ImGui::SameLine(ImGui::GetStyle().FramePadding.x * 2);
      ImGui::BeginGroup();
      ImGui::PushFont(fonts.bold, 0.0f);
      ImGui::TextUnformatted(store_label(g.store).c_str());
      ImGui::PopFont();
      ImGui::SameLine();
      ImGui::TextUnformatted(("   " + state).c_str());
      muted(g.path);
      ImGui::EndGroup();
      if (!g.supported) ImGui::EndDisabled();
      ImGui::PopID();
    }
    ImGui::Spacing();
    if (entered && chosen < 0) focus_here_once();   // no game to choose
    if (ImGui::Button("Look for another folder...")) pick(true, true);
    ImGui::SameLine();
    if (ImGui::Button("Browse with the pad...")) open_browser(true);
    if (!game_problem.empty()) error_text(game_problem);
    ImGui::EndChild();
    const bool ok = chosen >= 0 && chosen < int(games.size()) && games[size_t(chosen)].supported;
    if (footer("Next", ok, false)) next_from_games();
  }
  void next_from_games() {
    if (chosen < 0 || !games[size_t(chosen)].supported) return;
    game = games[size_t(chosen)];
    saves = saves_to_back_up(game.path);
    if (game.has_mod) {
      go(Screen::Action);
    } else {
      goal = Goal::Install;
      go(Screen::Package);
    }
  }

  void action_screen() {
    heading("The mod is already installed here", "1  Game   >   What to do");
    ImGui::BeginChild("body", ImVec2(0, body_height()), ImGuiChildFlags_NavFlattened);
    muted(game.path);
    wrapped(game.bag == "360" ? "It is installed with the 360's bag." : "It is installed with PC's bag.");
    ImGui::Spacing();
    const float w = ImGui::GetFontSize() * 16;
    focus_here_once();
    if (ImGui::Button("Reinstall", ImVec2(w, 0))) { goal = Goal::Reinstall; go(Screen::Package); }
    ImGui::SameLine();
    muted("Convert the 360's interface again, with the bag you choose.");
    if (ImGui::Button(game.bag == "360" ? "Change to PC's bag" : "Change to the 360's bag", ImVec2(w, 0))) {
      if (game.bag == "360") {
        goal = Goal::ToPcBag;
        bag = "pc";
        go(Screen::Summary);
      } else {
        goal = Goal::To360Bag;
        bag = "360";
        go(Screen::Bag);   // no package: only the bag (set-bag), its warning and backup
      }
    }
    ImGui::SameLine();
    muted(game.bag == "360" ? "Only the setting: then load and save each character." : "Only the setting; your saves are backed up first.");
    if (ImGui::Button("Uninstall", ImVec2(w, 0))) go(Screen::Uninstall);
    ImGui::SameLine();
    muted("Put the game back as it was.");
    ImGui::EndChild();
    footer(nullptr, false);
  }

  void uninstall_screen() {
    heading("Uninstall", "1  Game   >   Uninstall");
    ImGui::BeginChild("body", ImVec2(0, body_height()), ImGuiChildFlags_NavFlattened);
    muted(game.path);
    ImGui::Spacing();
    const float w = ImGui::GetFontSize() * 18;
    if (game.bag == "360") {
      warning_box("bag", "The 360's bag is installed", warning_text("no-mod"));
      ImGui::Spacing();
      wrapped("To uninstall without losing items:");
      wrapped("  1.  Switch to PC's bag (below).");
      wrapped("  2.  Start the game, load and save each character, and quit.");
      wrapped("  3.  Come back here and uninstall.");
      ImGui::Spacing();
      focus_here_once();
      if (ImGui::Button("Step 1: switch to PC's bag", ImVec2(w, 0))) { goal = Goal::UninstallStep1; go(Screen::Summary); }
      ImGui::Spacing();
      ImGui::Checkbox("I understand: uninstall now, and lose the items that do not fit PC's bag", &anyway_ok);
      if (!anyway_ok) ImGui::BeginDisabled();
      if (ImGui::Button("Uninstall anyway", ImVec2(w, 0))) { goal = Goal::UninstallAnyway; go(Screen::Summary); }
      if (!anyway_ok) ImGui::EndDisabled();
    } else if (game.bag_360_used) {
      warning_box("bag", "The 360's bag was used before", warning_text("no-mod-after"));
      ImGui::Spacing();
      focus_here_once();
      ImGui::Checkbox("I have loaded and saved each character with PC's bag", &loaded_each);
      if (!loaded_each) ImGui::BeginDisabled();
      if (ImGui::Button("Uninstall", ImVec2(w, 0))) { goal = Goal::UninstallAnyway; go(Screen::Summary); }
      if (!loaded_each) ImGui::EndDisabled();
    } else {
      wrapped("The real CEGUIBase.dll is put back and what the mod wrote is removed. Saves are not touched.");
      ImGui::Spacing();
      focus_here_once();
      if (ImGui::Button("Uninstall", ImVec2(w, 0))) { goal = Goal::Uninstall; go(Screen::Summary); }
    }
    ImGui::EndChild();
    footer(nullptr, false);
  }

  void package_screen() {
    heading("Your Xbox 360 package", "1  Game   >   2  Package   >   3  Bag   >   4  Summary");
    ImGui::BeginChild("body", ImVec2(0, body_height()), ImGuiChildFlags_NavFlattened);
    wrapped("Torchlight's Xbox LIVE Arcade package, copied from your console: a file without an extension, in "
            "Content\\0000000000000000\\58410A7E\\000D0000\\ on the console's drive. A folder extracted from it works too.");
    ImGui::Spacing();
    focus_here_once();
    if (ImGui::Button("Choose the package...")) pick(false, false);
    ImGui::SameLine();
    if (ImGui::Button("Choose a folder extracted from it...")) pick(false, true);
    // The wizard's own browser, played with the pad (where the system's picker does not show).
    if (ImGui::Button("Browse with the pad...")) open_browser(false);
    ImGui::Spacing();
    if (!package.empty()) {
      muted(package);
      if (package_problem_text.empty()) ImGui::TextColored(ImVec4(0.55f, 0.95f, 0.55f, 1.0f), "This is Torchlight's 360 package.");
      else error_text(package_problem_text);
    }
    ImGui::EndChild();
    const bool ok = !package.empty() && package_problem_text.empty();
    if (footer("Next", ok, true, ok)) go(Screen::Bag);   // the 360's bag's warning and backup there, always
  }

  void bag_screen() {
    heading("Which bag?", "1  Game   >   2  Package   >   3  Bag   >   4  Summary");
    ImGui::BeginChild("body", ImVec2(0, body_height()), ImGuiChildFlags_NavFlattened);
    const float h = ImGui::GetTextLineHeightWithSpacing() * 2 + ImGui::GetStyle().FramePadding.y;
    if (entered) {
      focus(bag == "pc" ? 0 : 1);
    }
    if (ImGui::Selectable("##pc", bag == "pc", 0, ImVec2(0, h))) bag = "pc";
    ImGui::SameLine(ImGui::GetStyle().FramePadding.x * 2);
    ImGui::BeginGroup();
    ImGui::PushFont(fonts.bold, 0.0f);
    ImGui::TextUnformatted("PC's bag (recommended)");
    ImGui::PopFont();
    muted("Three sections of 21 slots: any item, spells, fish. The game stays as it is without the mod.");
    ImGui::EndGroup();
    if (ImGui::Selectable("##360", bag == "360", 0, ImVec2(0, h))) bag = "360";
    ImGui::SameLine(ImGui::GetStyle().FramePadding.x * 2);
    ImGui::BeginGroup();
    ImGui::PushFont(fonts.bold, 0.0f);
    ImGui::TextUnformatted("The 360's bag");
    ImGui::PopFont();
    muted("One section of 50 slots for every kind of item, as on the console.");
    ImGui::EndGroup();
    ImGui::Spacing();
    if (bag == "360") {
      warning_box("warn", "Before you choose the 360's bag", warning_text("bags") + "\n\n" + warning_text("no-mod"));
      ImGui::Spacing();
      if (saves.empty()) {
        error_text(warning_text("no-saves"));
        ImGui::Checkbox("Install the 360's bag without a backup", &no_backup_ok);
      } else {
        wrapped("Your saves are backed up first, beside them:");
        for (const auto& f : saves) muted("    " + f);
      }
    }
    ImGui::EndChild();
    const bool ok = bag == "pc" || !saves.empty() || no_backup_ok;
    if (footer("Next", ok)) go(Screen::Summary);
  }

  std::vector<std::string> command() const {
    std::vector<std::string> a;
    const bool install = goal == Goal::Install || goal == Goal::Reinstall;
    if (goal == Goal::To360Bag) {
      a = {"set-bag", "--game", game.path, "--bag", bag, "--yes", "--console"};
      if (bag == "360" && saves.empty() && no_backup_ok) a.push_back("--no-backup-ok");
    } else if (install) {
      a = {"install", "--game", game.path, "--package", package, "--bag", bag, "--yes", "--console"};
      if (bag == "360" && saves.empty() && no_backup_ok) a.push_back("--no-backup-ok");
      if (!options.dll.empty()) a.insert(a.end(), {"--dll", options.dll});
    } else {
      a = {"uninstall", "--game", game.path, "--yes", "--console"};
      if (goal == Goal::UninstallAnyway) a.push_back("--anyway");
    }
    return a;
  }
  const char* goal_label() const {
    switch (goal) {
      case Goal::Install: return "Install the mod";
      case Goal::Reinstall: return "Install the mod again";
      case Goal::To360Bag: return bag == "360" ? "Switch to the 360's bag (nothing is converted again)" : "Keep PC's bag";
      case Goal::ToPcBag: case Goal::UninstallStep1: return "Switch to PC's bag (nothing is uninstalled)";
      case Goal::Uninstall: case Goal::UninstallAnyway: return "Uninstall the mod";
    }
    return "";
  }
  void summary_screen() {
    heading("Ready", "1  Game   >   2  Package   >   3  Bag   >   4  Summary");
    ImGui::BeginChild("body", ImVec2(0, body_height()), ImGuiChildFlags_NavFlattened);
    const bool install = goal == Goal::Install || goal == Goal::Reinstall;
    const bool to360 = goal == Goal::To360Bag;
    if (ImGui::BeginTable("summary", 2)) {
      ImGui::TableSetupColumn("key", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFontSize() * 6);
      ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);
      auto row = [&](const char* k, const std::string& v) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::PushFont(fonts.bold, 0.0f);
        ImGui::TextUnformatted(k);
        ImGui::PopFont();
        ImGui::TableNextColumn();
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextUnformatted(v.c_str());
        ImGui::PopTextWrapPos();
      };
      row("What", goal_label());
      row("Game", store_label(game.store) + ":  " + game.path);
      if (install) row("Package", package);
      if (install || to360) row("Bag", bag == "360" ? "The 360's (50 slots)" : "PC's (3 x 21 slots)");
      const bool backs_up = ((install || to360) && bag == "360") || goal == Goal::UninstallAnyway;
      if (backs_up) {
        std::string b = saves.empty() ? "No save folder was found: none is backed up." : "Your saves, beside them:";
        for (const auto& f : saves) b += "\n" + f;
        row("Backup", b);
      }
      if (needs_permission(game.path)) row("Permission", "Windows asks for an administrator's permission when you start.");
      ImGui::EndTable();
    }
    ImGui::EndChild();
    const char* label = install ? "Install" : (goal == Goal::ToPcBag || goal == Goal::UninstallStep1 || to360) ? "Switch bag" : "Uninstall";
    if (footer(label, true, true, true)) start();
  }

  void start() {
    result_text.clear();
    backups.clear();
    out.str("");
    err.str("");
    fraction = 0;
    progress_text = "Starting";
    cout_buf = std::cout.rdbuf(out.rdbuf());
    cerr_buf = std::cerr.rdbuf(err.rdbuf());
    set_progress([this](double f, const std::string& t) {
      std::lock_guard<std::mutex> lock(progress_mutex);
      fraction = f;
      progress_text = t;
    });
    working = true;
    worked = false;
    const std::vector<std::string> argv = command();
    worker = std::thread([this, argv] {
      code = installer_main(argv);
      worked = true;
    });
    history.clear();
    screen = Screen::Working;
    entered = true;
  }
  void finish_work() {
    if (!working) return;
    worker.join();
    working = false;
    set_progress(nullptr);
    std::cout.rdbuf(cout_buf);
    std::cerr.rdbuf(cerr_buf);
    // "saves backed up: F" here; an elevated run's result text lists them after its own line.
    bool listed = false;
    for (const std::string& l : text_lines(out.str())) {
      if (l.rfind("saves backed up: ", 0) == 0) {
        backups.push_back(l.substr(17));
      } else if (l == "Your saves were backed up:") {
        listed = true;
      } else if (listed) {
        if (l.empty()) listed = false;
        else if (std::find(backups.begin(), backups.end(), l) == backups.end()) backups.push_back(l);
      }
    }
    std::string e = err.str();
    while (!e.empty() && (e.back() == '\n' || e.back() == '\r')) e.pop_back();
    result_text = e;
    games = games_found();
    for (const auto& g : games)
      if (g.path == game.path) game = g;
  }
  void working_screen() {
    heading("Working", "Please wait: the game's folder is being changed");
    ImGui::BeginChild("body", ImVec2(0, body_height()), ImGuiChildFlags_NavFlattened);
    double f;
    std::string t;
    {
      std::lock_guard<std::mutex> lock(progress_mutex);
      f = fraction;
      t = progress_text;
    }
    ImGui::Spacing();
    ImGui::ProgressBar(float(f), ImVec2(-1, ImGui::GetFrameHeight() * 1.2f));
    wrapped(t);
    ImGui::EndChild();
    footer(nullptr, false, false);
    if (worked) {
      finish_work();
      go(Screen::Result);
      history.clear();
    }
  }

  void open_game() {
    if (game.store == "steam" || game.path.find("steamapps") != std::string::npos) {
      SDL_OpenURL("steam://rungameid/41500");
      return;
    }
#ifdef _WIN32
    const std::wstring exe = (fs_path(game.path) / "Torchlight.exe").wstring(), dir = fs_path(game.path).wstring();
    ShellExecuteW(nullptr, L"open", exe.c_str(), nullptr, dir.c_str(), SW_SHOWNORMAL);
#endif
  }
  bool can_open_game() const {
#ifdef _WIN32
    return true;
#else
    return game.store == "steam" || game.path.find("steamapps") != std::string::npos;
#endif
  }
  void result_screen() {
    const bool ok = code == 0;
    const bool installed = goal == Goal::Install || goal == Goal::Reinstall || (goal == Goal::To360Bag && bag == "360");
    const bool switched = goal == Goal::ToPcBag || goal == Goal::UninstallStep1;
    heading(!ok ? "It did not work" : goal == Goal::To360Bag ? (bag == "360" ? "The 360's bag installed" : "PC's bag kept")
                                    : installed ? "Installed" : switched ? "PC's bag installed" : "Uninstalled", "Done");
    ImGui::BeginChild("body", ImVec2(0, body_height()), ImGuiChildFlags_NavFlattened);
    if (!ok) {
      error_text(result_text.empty() ? "The installer stopped (status " + std::to_string(code) + ")." : result_text);
    } else {
      if (installed) wrapped("Start Torchlight with a pad connected.");
      if (switched) wrapped("Nothing was uninstalled. Start the game, load and save each character, and quit; then uninstall here.");
      if (!installed && !switched) wrapped("The game is as it was before the mod. Saves were not touched.");
      if (!backups.empty()) {
        ImGui::Spacing();
        wrapped("Your saves were backed up:");
        for (const auto& b : backups) muted("    " + b);
      }
    }
    ImGui::Spacing();
    if (ImGui::CollapsingHeader("Details")) {
      ImGui::PushTextWrapPos(0.0f);
      ImGui::TextUnformatted(out.str().c_str());
      ImGui::PopTextWrapPos();
    }
    ImGui::EndChild();
    ImGui::Separator();
    focus_here_once();
    if (ok && (installed || switched) && can_open_game()) {
      if (ImGui::Button("Open the game")) open_game();
      ImGui::SameLine();
    }
    // Start: start again (A takes the focused button, Open the game where there is one).
    if (ImGui::Button("Start again") || (!entered && ImGui::IsKeyPressed(ImGuiKey_GamepadStart, false))) restart();
    ImGui::SameLine();
    if (ImGui::Button("Close")) closed = true;
    hint_line(true);
  }

  void browser_screen() {
    heading(browse_for_game ? "Choose the game's folder" : "Choose the package, or a folder extracted from it",
            "Your files");
    muted(browse_drives ? "This computer" : utf8(browse_dir));
    ImGui::BeginChild("body", ImVec2(0, body_height() - ImGui::GetFrameHeightWithSpacing()),
                      ImGuiChildFlags_Borders | ImGuiChildFlags_NavFlattened);
    int i = 0;
    if (!browse_drives) {
      if (entered) focus();
      if (ImGui::Selectable("..  (the folder above)")) {
        const fs::path up = browse_dir.parent_path();
#ifdef _WIN32
        if (up == browse_dir) list_drives();
        else list(up);
#else
        list(up.empty() ? browse_dir : up);
#endif
      }
    }
    std::vector<Entry> shown = entries;
    for (const Entry& e : shown) {
      ImGui::PushID(i++);
      if (browse_drives && entered && i == 1) focus();
      if (ImGui::Selectable(((e.folder ? "[folder]  " : "            ") + e.name).c_str())) {
        if (e.folder) {
          browse_drives = false;
          list(e.path);
        } else {
          chose(false, utf8(e.path));
          back();
        }
      }
      ImGui::PopID();
    }
    if (!browse_error.empty()) error_text(browse_error);
    ImGui::EndChild();
    if (!browse_drives && ImGui::Button("Use this folder")) {
      chose(browse_for_game, utf8(browse_dir));
      back();
    }
    footer(nullptr, false);
  }
};

Screens::Screens(SDL_Window* window, Fonts fonts, const Options& options) : s_(new State(window, fonts, options)) {}
Screens::~Screens() { finish(); }

bool Screens::frame() {
  State& s = *s_;
  s.poll_pick();
  const ImGuiIO& io = ImGui::GetIO();
  ImGui::SetNextWindowPos(ImVec2(0, 0));
  ImGui::SetNextWindowSize(io.DisplaySize);
  ImGui::Begin("##wizard", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                   ImGuiWindowFlags_NoBringToFrontOnFocus);
  const Screen before = s.screen;
  const bool first = s.entered && s.entered_frames > 0;
  // B or Escape: back (not while it works, nor from the first screen or the result).
  if (!first && !s.working && s.screen != Screen::Result &&
      (ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false) || ImGui::IsKeyPressed(ImGuiKey_Escape, false)))
    s.back();
  switch (s.screen) {
    case Screen::Games: s.games_screen(); break;
    case Screen::Action: s.action_screen(); break;
    case Screen::Uninstall: s.uninstall_screen(); break;
    case Screen::Package: s.package_screen(); break;
    case Screen::Bag: s.bag_screen(); break;
    case Screen::Summary: s.summary_screen(); break;
    case Screen::Working: s.working_screen(); break;
    case Screen::Result: s.result_screen(); break;
    case Screen::Browser: s.browser_screen(); break;
  }
  ImGui::End();
  // The focus asked for on a screen's first two frames (ImGui takes it from the second).
  if (s.screen != before) s.entered_frames = 0;
  else if (s.entered && ++s.entered_frames >= 2) s.entered = false;
  else if (!s.entered) s.entered_frames = 0;
  return s.closed;
}

std::string Screens::name() const { return screen_name(s_->screen); }
bool Screens::busy() const {
#ifdef _WIN32
  // Windows' picker answers from its own thread into this window's state: not closed under it.
  std::lock_guard<std::mutex> lock(s_->pick_mutex);
  if (s_->pick_pending && !s_->pick_answered) return true;
#endif
  return s_->working;   // Linux's portal answers on this thread, while the window's events are read
}
void Screens::finish() {
  if (s_->working) {
    while (!s_->worked) SDL_Delay(10);
    s_->finish_work();
  }
}

}  // namespace x360ui::wizard
