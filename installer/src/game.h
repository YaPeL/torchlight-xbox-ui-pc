// The PC game: finding its installs, telling its build, checking its CEGUIBase.dll (the C++ port of
// tools/install.py's "the PC game" part, tools/build_proxy.py's is_proxy and
// tools/cegui_exports.py's check).
#pragma once
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "bytes.h"

namespace x360ui {

constexpr uint32_t kGogTimestamp = 0x4FFC6815;     // Torchlight.exe of GOG's build 17870 (also Epic's)
constexpr uint32_t kSteamTimestamp = 0x4B5F7519;   // Steam's build 17685

bool is_game(const std::string& path);
uint32_t exe_timestamp(const std::string& game);   // Error when unreadable
std::string build_of(const std::string& game);     // "gog", "steam" or "unknown"
bool is_experimental(const std::string& build);    // installed only into a --game given, after a yes: none now

// [(store, path)] of the installs found: GOG (Heroic, Lutris, Wine prefixes, the usual folders,
// GOG Galaxy's registry), Steam (its libraries), Epic (Legendary, Heroic, the launcher's
// manifests), in Python's order.
std::vector<std::pair<std::string, std::string>> find_games(const std::string& home);

bool is_proxy(const std::string& dll_path);        // the DLL forwards to CEGUIBase_orig.
std::vector<std::pair<std::string, uint32_t>> pe_exports(const Bytes& pe);
// SystemExit-like Error with the user's message unless the real CEGUIBase.dll exports exactly
// proxy/cegui_exports.txt (built in).
void check_real_dll(const std::string& game);
// The built-in proxy/cegui_exports.txt's (name, ordinal), in its order.
std::vector<std::pair<std::string, uint32_t>> exports_list();

// A tiny JSON reader for the launchers' files (objects, arrays, strings, numbers, true, false, null).
struct JsonValue {
  enum Kind { Null, Bool, Number, String, Array, Object } kind = Null;
  bool b = false;
  double n = 0;
  std::string s;
  std::vector<JsonValue> items;
  std::vector<std::pair<std::string, JsonValue>> fields;
  const JsonValue* get(const std::string& key) const;
};
bool parse_json(const std::string& text, JsonValue& out);

}  // namespace x360ui
