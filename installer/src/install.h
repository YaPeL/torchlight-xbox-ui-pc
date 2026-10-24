// The installer: the C++ port of tools/install.py (install, uninstall, list), with what the owner
// asked of the C++ one: on Windows the game's folder tested for writing before anything and UAC
// asked for only then (no write ever redirected: the executable's manifest is asInvoker), and the
// changes to the game's folder made as one step that is undone if any part of it fails.
#pragma once
#include <functional>
#include <string>
#include <vector>

namespace x360ui {

int installer_main(const std::vector<std::string>& args);
// Windows: the executable, a window program, writes to the console of the terminal it was run from
// (main.cpp). That console's input is the shell's too, so --console cannot ask questions there.
void set_shared_console(bool shared);

// ---- what the wizard asks before it runs installer_main with every answer given (--yes) ----

struct GameFound {
  std::string store;          // "steam", "epic", "gog"; empty for a folder the player chose
  std::string path;           // empty when the folder chosen is not a Torchlight
  bool supported = false;     // a build this version knows (its proxy: built in, or --dll)
  bool has_mod = false;
  std::string bag;            // the bag installed ("pc" or "360"), when has_mod
  bool bag_360_used = false;  // the 360's bag was installed at some point
};
// The installs found, Steam's first and then the others in the order found (install.py's).
std::vector<GameFound> games_found();
// A folder, or a file in it (its Torchlight.exe), the player chose.
GameFound game_in(const std::string& chosen);
// Why the 360 copy (the package, or a file of a folder extracted from it) cannot be used; empty
// when it can (its header and the files it must hold: no extraction, no decryption).
std::string package_problem(const std::string& source);
// The save folders the 360's bag would back up (install.save_folders).
std::vector<std::string> saves_to_back_up(const std::string& game);
// The game's folder cannot be written as the user (Windows asks for UAC when installing).
bool needs_permission(const std::string& game);
// installer_main's progress, on its thread: a fraction (0 to 1) and what it is doing. An elevated
// run's progress comes through a file and is given here too.
using Progress = std::function<void(double fraction, const std::string& text)>;
void set_progress(Progress progress);

}  // namespace x360ui
