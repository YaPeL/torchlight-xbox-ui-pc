// The installer's window (the owner's design, 2026-10-24): one window with Back and Next, the
// same on Windows and Linux, played with a pad, a mouse or a keyboard. It asks what install.h
// answers without questions, then runs installer_main with every answer given (--yes), so what
// it installs is what the command line installs.
#pragma once
#include <string>

namespace x360ui::wizard {

struct Options {
  int width = 960, height = 600;   // the window's size in points (times the scale in pixels)
  float scale = 0;                 // 0: the display's
  std::string video;               // SDL's video driver ("offscreen" for the tests); empty: SDL's
  bool software = false;           // SDL's software renderer (the same pixels everywhere)
  bool own_picker = false;         // the wizard's own folder browser, never the system's picker
  bool fullscreen = false;         // the Steam Deck's (a display of 1280x800 or less) also is
  std::string script;              // a test's steps: the simulated pad, keys, screenshots
  std::string shots;               // the folder the script's screenshots go to (PNG)
  std::string browse_from;         // the own browser's first folder (the tests'); empty: home
  std::string dll;                 // a proxy to install (the tests', as --dll); empty: the built-in
};

// Opens the window and runs until it is closed. 0 when it ran (whatever was installed), 2 when
// no window could be opened (the caller falls back to the console), 3 when a script failed.
int run(const Options& options);

}  // namespace x360ui::wizard
