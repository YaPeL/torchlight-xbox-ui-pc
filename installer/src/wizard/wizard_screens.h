// The wizard's screens, drawn each frame inside the window app.cpp opens.
#pragma once
#include <SDL3/SDL.h>

#include <memory>
#include <string>

#include "imgui.h"
#include "wizard.h"

namespace x360ui::wizard {

inline constexpr const char* kTitle = "Torchlight: the Xbox 360 interface";
inline constexpr float kTextSize = 19.0f;   // points: times the display's scale in pixels

struct Fonts {
  ImFont* regular = nullptr;
  ImFont* bold = nullptr;
};

class Screens {
 public:
  Screens(SDL_Window* window, Fonts fonts, const Options& options);
  ~Screens();
  // Draws this frame's screen; true once the player closed the wizard.
  bool frame();
  // The screen shown ("games", "action", "uninstall", "package", "bag", "summary", "working",
  // "result", "browser"): the tests' scripts wait for one.
  std::string name() const;
  // An install or an uninstall under way (the window is not closed then).
  bool busy() const;
  // Waits for an install under way.
  void finish();

 private:
  struct State;
  std::unique_ptr<State> s_;
};

}  // namespace x360ui::wizard
