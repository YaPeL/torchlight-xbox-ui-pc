// The installer: the C++ port of tools/install.py (install, uninstall, list), with what the owner
// asked of the C++ one: on Windows the game's folder tested for writing before anything and UAC
// asked for only then (no write ever redirected: the executable's manifest is asInvoker), and the
// changes to the game's folder made as one step that is undone if any part of it fails.
#pragma once
#include <string>
#include <vector>

namespace x360ui {

int installer_main(const std::vector<std::string>& args);
// Windows: the executable, a window program, writes to the console of the terminal it was run from
// (main.cpp). That console's input is the shell's too, so --console cannot ask questions there.
void set_shared_console(bool shared);

}  // namespace x360ui
