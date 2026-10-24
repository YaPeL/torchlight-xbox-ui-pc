// The wizard's window: SDL3 and Dear ImGui (its SDL3 platform and SDL_Renderer backends), the
// theme, the fonts, the scale, the simulated pad of the tests' scripts and their screenshots.
#include <SDL3/SDL.h>

#include <cstdio>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"
#include "miniz.h"
#include "text_lines.h"
#include "wizard.h"
#include "wizard_screens.h"

namespace x360ui {
extern const unsigned char* const kFontRegular;
extern const size_t kFontRegularSize;
extern const unsigned char* const kFontBold;
extern const size_t kFontBoldSize;
}  // namespace x360ui

namespace x360ui::wizard {
namespace {

// The ReXGlue SDK's ImGui palette (its src/ui/imgui_drawer.cpp, BSD-3-Clause, from Xenia: the
// notice is in installer/third_party/NOTICES.md), the recomp's look, with Noto Sans for a text
// that reads at the wizard's size.
void apply_theme(ImGuiStyle& style, float scale) {
  ImGui::StyleColorsDark(&style);
  ImVec4* c = style.Colors;
  c[ImGuiCol_Text] = ImVec4(0.89f, 0.90f, 0.90f, 1.00f);
  c[ImGuiCol_TextDisabled] = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
  c[ImGuiCol_WindowBg] = ImVec4(0.00f, 0.06f, 0.00f, 1.00f);
  c[ImGuiCol_ChildBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
  c[ImGuiCol_Border] = ImVec4(0.00f, 0.35f, 0.00f, 1.00f);
  c[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
  c[ImGuiCol_FrameBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.30f);
  c[ImGuiCol_FrameBgHovered] = ImVec4(0.90f, 0.80f, 0.80f, 0.40f);
  c[ImGuiCol_FrameBgActive] = ImVec4(0.90f, 0.65f, 0.65f, 0.45f);
  c[ImGuiCol_TitleBg] = ImVec4(0.00f, 0.40f, 0.00f, 1.00f);
  c[ImGuiCol_TitleBgCollapsed] = ImVec4(0.00f, 0.33f, 0.00f, 1.00f);
  c[ImGuiCol_TitleBgActive] = ImVec4(0.16f, 0.65f, 0.00f, 1.00f);
  c[ImGuiCol_MenuBarBg] = ImVec4(0.00f, 0.35f, 0.00f, 1.00f);
  c[ImGuiCol_ScrollbarBg] = ImVec4(0.00f, 0.40f, 0.11f, 0.59f);
  c[ImGuiCol_ScrollbarGrab] = ImVec4(0.00f, 0.68f, 0.00f, 0.68f);
  c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.00f, 1.00f, 0.15f, 0.62f);
  c[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.00f, 0.91f, 0.09f, 0.40f);
  c[ImGuiCol_PopupBg] = ImVec4(0.20f, 0.20f, 0.20f, 0.99f);
  c[ImGuiCol_CheckMark] = ImVec4(0.74f, 0.90f, 0.72f, 0.50f);
  c[ImGuiCol_SliderGrab] = ImVec4(1.00f, 1.00f, 1.00f, 0.30f);
  c[ImGuiCol_SliderGrabActive] = ImVec4(0.34f, 0.75f, 0.11f, 1.00f);
  c[ImGuiCol_Button] = ImVec4(0.15f, 0.56f, 0.11f, 0.60f);
  c[ImGuiCol_ButtonHovered] = ImVec4(0.19f, 0.72f, 0.09f, 1.00f);
  c[ImGuiCol_ButtonActive] = ImVec4(0.19f, 0.60f, 0.09f, 1.00f);
  c[ImGuiCol_Header] = ImVec4(0.00f, 0.40f, 0.00f, 0.71f);
  c[ImGuiCol_HeaderHovered] = ImVec4(0.00f, 0.60f, 0.26f, 0.80f);
  c[ImGuiCol_HeaderActive] = ImVec4(0.00f, 0.75f, 0.00f, 0.80f);
  c[ImGuiCol_Separator] = ImVec4(0.00f, 0.35f, 0.00f, 1.00f);
  c[ImGuiCol_SeparatorHovered] = ImVec4(0.36f, 0.89f, 0.38f, 1.00f);
  c[ImGuiCol_SeparatorActive] = ImVec4(0.13f, 0.50f, 0.11f, 1.00f);
  c[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
  c[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
  c[ImGuiCol_TextSelectedBg] = ImVec4(0.00f, 1.00f, 0.00f, 0.21f);
  c[ImGuiCol_ModalWindowDimBg] = ImVec4(0.20f, 0.20f, 0.20f, 0.35f);
  // The wizard's own: the pad's focus, as visible as the SDK's hovered button.
  c[ImGuiCol_NavCursor] = ImVec4(0.36f, 0.89f, 0.38f, 1.00f);
  style.WindowRounding = 0;
  style.ScrollbarRounding = 0;
  style.TabRounding = 0;
  style.FrameRounding = 4;
  style.ChildRounding = 4;
  style.WindowPadding = ImVec2(28, 22);
  style.FramePadding = ImVec2(14, 8);
  style.ItemSpacing = ImVec2(12, 10);
  style.ScrollbarSize = 16;
  style.ScaleAllSizes(scale);
  style.FontSizeBase = kTextSize;
  style.FontScaleDpi = scale;
}

// ---- the tests' scripts: one step per line (wait N, press BUTTON, key KEY, text TEXT, shot NAME,
// screen NAME [SECONDS], quit) ----
struct Step {
  std::string op, arg;
  double seconds = 0;
};

std::vector<Step> read_script(const std::string& path, std::string& error) {
  std::vector<Step> steps;
  std::ifstream f(path, std::ios::binary);
  if (!f) {
    error = "cannot read the script " + path;
    return steps;
  }
  const std::string all((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
  for (std::string line : text_lines(all)) {
    while (!line.empty() && line.back() == ' ') line.pop_back();
    if (line.empty() || line[0] == '#') continue;
    Step s;
    const size_t sp = line.find(' ');
    s.op = line.substr(0, sp);
    if (sp != std::string::npos) s.arg = line.substr(sp + 1);
    if (s.op == "screen") {
      const size_t sp2 = s.arg.find(' ');
      s.seconds = sp2 == std::string::npos ? 30 : std::atof(s.arg.c_str() + sp2 + 1);
      if (sp2 != std::string::npos) s.arg = s.arg.substr(0, sp2);
    }
    steps.push_back(s);
  }
  return steps;
}

int button_of(const std::string& name) {
  static const std::pair<const char*, int> names[] = {
      {"a", SDL_GAMEPAD_BUTTON_SOUTH},    {"b", SDL_GAMEPAD_BUTTON_EAST},
      {"x", SDL_GAMEPAD_BUTTON_WEST},     {"y", SDL_GAMEPAD_BUTTON_NORTH},
      {"up", SDL_GAMEPAD_BUTTON_DPAD_UP}, {"down", SDL_GAMEPAD_BUTTON_DPAD_DOWN},
      {"left", SDL_GAMEPAD_BUTTON_DPAD_LEFT}, {"right", SDL_GAMEPAD_BUTTON_DPAD_RIGHT},
      {"start", SDL_GAMEPAD_BUTTON_START}, {"back", SDL_GAMEPAD_BUTTON_BACK}};
  for (const auto& [n, b] : names)
    if (name == n) return b;
  return -1;
}

ImGuiKey key_of(const std::string& name) {
  static const std::pair<const char*, ImGuiKey> names[] = {
      {"enter", ImGuiKey_Enter}, {"escape", ImGuiKey_Escape}, {"tab", ImGuiKey_Tab},
      {"up", ImGuiKey_UpArrow},  {"down", ImGuiKey_DownArrow}, {"left", ImGuiKey_LeftArrow},
      {"right", ImGuiKey_RightArrow}, {"space", ImGuiKey_Space}, {"backspace", ImGuiKey_Backspace}};
  for (const auto& [n, k] : names)
    if (name == n) return k;
  return ImGuiKey_None;
}

// The renderer's pixels as a PNG (miniz's writer).
bool save_png(SDL_Renderer* renderer, const std::string& path) {
  SDL_Surface* shot = SDL_RenderReadPixels(renderer, nullptr);
  if (!shot) return false;
  SDL_Surface* rgba = SDL_ConvertSurface(shot, SDL_PIXELFORMAT_RGBA32);
  SDL_DestroySurface(shot);
  if (!rgba) return false;
  std::vector<unsigned char> pixels(size_t(rgba->w) * size_t(rgba->h) * 4);
  for (int y = 0; y < rgba->h; y++)
    SDL_memcpy(pixels.data() + size_t(y) * size_t(rgba->w) * 4, static_cast<unsigned char*>(rgba->pixels) + size_t(y) * size_t(rgba->pitch),
               size_t(rgba->w) * 4);
  size_t size = 0;
  void* png = tdefl_write_image_to_png_file_in_memory_ex(pixels.data(), rgba->w, rgba->h, 4, &size, 6, MZ_FALSE);
  SDL_DestroySurface(rgba);
  if (!png) return false;
  std::ofstream out(path, std::ios::binary);
  out.write(static_cast<const char*>(png), std::streamsize(size));
  mz_free(png);
  return bool(out);
}

}  // namespace

int run(const Options& options) {
  if (!options.video.empty()) SDL_SetHint(SDL_HINT_VIDEO_DRIVER, options.video.c_str());
  if (options.software) SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");
  // Linux: the desktop portal's pickers only (no zenity); where there is none (the Steam Deck's
  // Game Mode) the wizard's own browser.
  SDL_SetHint(SDL_HINT_FILE_DIALOG_DRIVER, "portal");
  SDL_SetHint(SDL_HINT_APP_NAME, kTitle);
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
    std::fprintf(stderr, "no window: %s\n", SDL_GetError());
    return 2;
  }
  float scale = options.scale > 0 ? options.scale : SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
  if (scale <= 0) scale = 1;
  bool fullscreen = options.fullscreen;
  if (options.scale <= 0 && options.script.empty()) {
    SDL_Rect usable;
    if (SDL_GetDisplayUsableBounds(SDL_GetPrimaryDisplay(), &usable) && usable.w <= 1280 && usable.h <= 800) fullscreen = true;
  }
  SDL_Window* window = SDL_CreateWindow(kTitle, int(float(options.width) * scale), int(float(options.height) * scale),
                                        SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY |
                                            (fullscreen ? SDL_WINDOW_FULLSCREEN : 0));
  if (!window) {
    std::fprintf(stderr, "no window: %s\n", SDL_GetError());
    SDL_Quit();
    return 2;
  }
  SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
  if (!renderer) {
    std::fprintf(stderr, "no renderer: %s\n", SDL_GetError());
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 2;
  }
  SDL_SetRenderVSync(renderer, 1);
  SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
  SDL_ShowWindow(window);

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.IniFilename = nullptr;   // nothing written beside the executable
  io.LogFilename = nullptr;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
  apply_theme(ImGui::GetStyle(), scale);
  ImFontConfig cfg;
  cfg.FontDataOwnedByAtlas = false;   // the fonts are the executable's
  Fonts fonts;
  fonts.regular = io.Fonts->AddFontFromMemoryTTF(const_cast<unsigned char*>(kFontRegular), int(kFontRegularSize), kTextSize, &cfg);
  fonts.bold = io.Fonts->AddFontFromMemoryTTF(const_cast<unsigned char*>(kFontBold), int(kFontBoldSize), kTextSize, &cfg);
  ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
  ImGui_ImplSDLRenderer3_Init(renderer);

  // The script's simulated pad: a virtual gamepad SDL lists as any other.
  std::string error;
  std::vector<Step> steps;
  SDL_Joystick* pad = nullptr;
  if (!options.script.empty()) {
    steps = read_script(options.script, error);
    SDL_VirtualJoystickDesc desc;
    SDL_INIT_INTERFACE(&desc);
    desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
    desc.naxes = SDL_GAMEPAD_AXIS_COUNT;
    desc.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
    desc.name = "x360ui test pad";
    const SDL_JoystickID id = SDL_AttachVirtualJoystick(&desc);
    SDL_Gamepad* gamepad = id ? SDL_OpenGamepad(id) : nullptr;
    pad = gamepad ? SDL_GetGamepadJoystick(gamepad) : nullptr;
    if (!pad) error = std::string("no simulated pad: ") + SDL_GetError();
    // ImGui hears this pad only (a real one connected to the machine would be the first).
    else ImGui_ImplSDL3_SetGamepadMode(ImGui_ImplSDL3_GamepadMode_Manual, &gamepad, 1);
  }

  Screens screens(window, fonts, options);
  size_t next = 0;
  int wait = 3, held = -1, hold_frames = 0;
  Uint64 screen_since = 0;
  bool quit = !error.empty();
  int status = error.empty() ? 0 : 3;
  while (!quit) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      ImGui_ImplSDL3_ProcessEvent(&e);
      if (e.type == SDL_EVENT_QUIT || (e.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && e.window.windowID == SDL_GetWindowID(window)))
        quit = !screens.busy();   // not while it installs
    }
    if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED) {
      SDL_Delay(10);
      continue;
    }
    bool shot_now = false;
    std::string shot_name;
    // One script step when the last is done (a press: three frames held, three released).
    if (pad && held >= 0 && --hold_frames <= 0) {
      if (held < 1000) {
        SDL_SetJoystickVirtualButton(pad, held, false);
        held = 1000;
        hold_frames = 3;
      } else {
        held = -1;
      }
    } else if (held < 0 && --wait <= 0 && next < steps.size()) {
      const Step& s = steps[next];
      wait = 1;
      if (s.op == "wait") {
        wait = std::max(1, std::atoi(s.arg.c_str()));
        next++;
      } else if (s.op == "press") {
        const int b = button_of(s.arg);
        if (b < 0) { error = "no button " + s.arg; quit = true; status = 3; }
        else { SDL_SetJoystickVirtualButton(pad, b, true); held = b; hold_frames = 3; }
        next++;
      } else if (s.op == "down" || s.op == "up") {   // a button held down, or let go
        const int b = button_of(s.arg);
        if (b < 0) { error = "no button " + s.arg; quit = true; status = 3; }
        else SDL_SetJoystickVirtualButton(pad, b, s.op == "down");
        next++;
      } else if (s.op == "key") {
        const ImGuiKey k = key_of(s.arg);
        if (k == ImGuiKey_None) { error = "no key " + s.arg; quit = true; status = 3; }
        else { io.AddKeyEvent(k, true); io.AddKeyEvent(k, false); }
        wait = 3;
        next++;
      } else if (s.op == "text") {
        io.AddInputCharactersUTF8(s.arg.c_str());
        wait = 3;
        next++;
      } else if (s.op == "shot") {
        shot_now = true;
        shot_name = s.arg;
        next++;
      } else if (s.op == "shot-if") {   // shot-if SCREEN NAME: only while that screen shows
        const size_t sp = s.arg.find(' ');
        if (screens.name() == s.arg.substr(0, sp)) {
          shot_now = true;
          shot_name = s.arg.substr(sp + 1);
        }
        next++;
      } else if (s.op == "screen") {
        if (!screen_since) screen_since = SDL_GetTicks();
        if (screens.name() == s.arg) {
          screen_since = 0;
          wait = 4;   // drawn twice before the next step (ImGui settles its layout)
          next++;
        } else if (double(SDL_GetTicks() - screen_since) > s.seconds * 1000) {
          error = "the screen " + s.arg + " was not reached (still " + screens.name() + ")";
          quit = true;
          status = 3;
        }
      } else if (s.op == "debug") {   // ImGui's navigation, for a script being written
        ImGuiContext& g = *ImGui::GetCurrentContext();
        std::fprintf(stderr, "debug: screen %s nav id %08X window %s gamepad %d nav active %d focused %d\n",
                     screens.name().c_str(), g.NavId, g.NavWindow ? g.NavWindow->Name : "-",
                     (io.BackendFlags & ImGuiBackendFlags_HasGamepad) ? 1 : 0, io.NavActive ? 1 : 0,
                     (SDL_GetWindowFlags(window) & SDL_WINDOW_INPUT_FOCUS) ? 1 : 0);
        if (pad) {
          SDL_Gamepad* gp = SDL_GetGamepadFromID(SDL_GetJoystickID(pad));
          std::fprintf(stderr, "debug: pad is gamepad %d, joystick button 0 %d, gamepad south %d, imgui facedown %d\n",
                       SDL_IsGamepad(SDL_GetJoystickID(pad)) ? 1 : 0, SDL_GetJoystickButton(pad, 0) ? 1 : 0,
                       gp && SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_SOUTH) ? 1 : 0,
                       ImGui::IsKeyDown(ImGuiKey_GamepadFaceDown) ? 1 : 0);
          if (gp) {
            char* m = SDL_GetGamepadMapping(gp);
            std::fprintf(stderr, "debug: mapping %s\n", m ? m : "-");
            SDL_free(m);
          }
        }
        next++;
      } else if (s.op == "quit") {
        quit = true;
      } else {
        error = "no step " + s.op;
        quit = true;
        status = 3;
      }
    } else if (pad && held < 0 && next >= steps.size()) {
      quit = true;   // a script that ends without quit
    }

    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
    if (screens.frame()) quit = true;
    ImGui::Render();
    SDL_SetRenderScale(renderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
    SDL_SetRenderDrawColorFloat(renderer, 0.0f, 0.06f, 0.0f, 1.0f);
    SDL_RenderClear(renderer);
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
    if (shot_now) {
      const std::string path = options.shots + "/" + shot_name + ".png";
      if (options.shots.empty() || !save_png(renderer, path)) {
        error = "cannot write the screenshot " + path;
        quit = true;
        status = 3;
      }
    }
    SDL_RenderPresent(renderer);
  }
  if (!error.empty()) std::fprintf(stderr, "wizard: %s\n", error.c_str());
  screens.finish();
  ImGui_ImplSDLRenderer3_Shutdown();
  ImGui_ImplSDL3_Shutdown();
  ImGui::DestroyContext();
  if (pad) SDL_CloseGamepad(SDL_GetGamepadFromID(SDL_GetJoystickID(pad)));
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return status;
}

}  // namespace x360ui::wizard
