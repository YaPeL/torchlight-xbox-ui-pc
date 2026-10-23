// The installer: the C++ port of tools/install.py (install, uninstall, list), with what the owner
// asked of the C++ one: on Windows the game's folder tested for writing before anything and UAC
// asked for only then (no write ever redirected: the executable's manifest is asInvoker), and the
// changes to the game's folder made as one step that is undone if any part of it fails.
#pragma once
#include <string>
#include <vector>

namespace x360ui {

int installer_main(const std::vector<std::string>& args);

}  // namespace x360ui
