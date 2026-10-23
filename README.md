# Torchlight: the Xbox 360 interface on PC

[![CI](https://github.com/YaPeL/torchlight-xbox-ui-pc/actions/workflows/ci.yml/badge.svg)](https://github.com/YaPeL/torchlight-xbox-ui-pc/actions/workflows/ci.yml)
[![Latest release](https://img.shields.io/github/v/release/YaPeL/torchlight-xbox-ui-pc?include_prereleases&sort=semver)](https://github.com/YaPeL/torchlight-xbox-ui-pc/releases)
[![License: GPL-3.0](https://img.shields.io/badge/license-GPL--3.0-blue)](LICENSE)
[![GOG](https://img.shields.io/badge/GOG-supported-86328A?logo=gogdotcom&logoColor=white)](#installing)
[![Epic Games Store](https://img.shields.io/badge/Epic%20Games%20Store-supported-313131?logo=epicgames&logoColor=white)](#installing)
[![Steam](https://img.shields.io/badge/Steam-planned-1b2838?logo=steam&logoColor=white)](#known-limitations)

A mod for the PC version of Torchlight that replaces its mouse interface with the Xbox 360
version's: the 360's menus, HUD, shops, dialogs and fishing, played with a game pad as on the
console. It loads as a proxy `CEGUIBase.dll` next to the game and converts the 360's interface
files from your own copy of the Xbox LIVE Arcade game.

> [!IMPORTANT]
> This repository contains no game files, from the Xbox 360 or from PC. The mod only works with
> your own copies of both: the PC game installed, and the Torchlight Xbox LIVE Arcade package from
> your Xbox 360. This project is not affiliated with or endorsed by the owners of Torchlight or by
> Microsoft.

This is a **beta**, tested under Linux (Proton). See
[Known limitations](#known-limitations).

## Contents

- [Installing](#installing)
- [Playing](#playing)
- [What works](#what-works)
- [Known limitations](#known-limitations)
- [Tools](#tools)
- [Building](#building)
- [Credits](#credits)
- [License](#license)
- [Other versions](#other-versions)

## Installing

You need:

- Torchlight for PC, from **GOG** or the **Epic Games Store**. Steam's build is not supported yet.
- Your Torchlight Xbox LIVE Arcade package (title 58410A7E, version 1.0.140.0), the file from your
  Xbox 360's storage, or a folder extracted from it.
- Python 3. Nothing else: the package is read and its executable decrypted in plain Python
  (pycryptodome, if installed, makes it faster).

Run `python3 tools/install.py`. It finds the PC game (GOG, Steam and Epic installs, also inside
Wine and Proton prefixes), asks for your 360 package, checks that it is Torchlight's, converts the
360's interface into the game's folder (one copy per pad style: Xbox, PlayStation 3/4/5 and
Nintendo), writes `x360_ui.ini` and installs the proxy. The game's real `CEGUIBase.dll` is kept as
`CEGUIBase_orig.dll`. It takes about a minute.

It also asks which bag you want:

- **PC's** (the default): three sections of 21 slots.
- **The 360's**: one section of 50 slots for every kind of item, as on the console. Your saves keep
  each item's slot, so with it items past the 50th slot of a save are lost, and going back to PC's
  bag later places items oddly. The installer backs up the saves it finds first.

Options: `--game DIR`, `--package FILE` or `--x360-dir DIR`, `--bag pc|360`, `--yes` (no
questions, PC's bag), `--console` (questions on the console, not in windows).

- `python3 tools/install.py uninstall` puts the real `CEGUIBase.dll` back and removes what the
  install wrote. Saves are not touched.
- `python3 tools/install.py list` prints the games it finds.

## Playing

Start the game with a pad connected. The mouse is hidden, except in PC's settings and character
creation screens.

| Pad | In play |
|-----|---------|
| Left stick | Move |
| Right stick | Zoom |
| A | Pick up, talk, use |
| X | Attack |
| B, Y, LT, RT | Skills |
| LB, RB | Health and mana potions |
| Back | Inventory |
| Start | Pause menu |
| Right stick click | The map's three views |

PC's main menu, pause menu, settings, character creation and loading screens are used with the
pad too: the d-pad or the stick moves between their buttons, A presses, B goes back. On a slider
or a drop-down, A starts changing it with left and right.

The button prompts follow the connected pad: Xbox, PlayStation 3, 4 or 5, or Nintendo.

## What works

- The HUD: health and mana orbs, the skill and potion slots with their prompts, the experience
  bar, the 360's minimap with its three views.
- The inventory: the hero's bag, the pet's, the character sheet, skills, the pet sheet and its
  commands, using and equipping items, identifying them.
- Shops: the merchant (buy and sell), the stash, the transmuter, the enchanter and both gem
  goblins, with the 360's pages and results.
- NPC dialogs, quest dialogs, tips and messages, as the 360's.
- Fishing, with the 360's ring.
- Labels on the floor's items near the hero, as on the 360.

## Known limitations

- **GOG and the Epic Games Store only.** Steam sells an older build (17685): every address the
  mod uses is already found in it, and its test is still to come.
- **The bag.** With PC's bag (the default) the equipment section holds 21 items; the 360's bag
  is an install option (above). Changing the bag of a save that has more items than the new bag
  holds: the game says so and stops without saving (nothing is lost); go back to the other bag and
  make room. A check in the installer, before changing the bag, is still to come.
- Windows-only binaries; on Linux the game runs under Proton or Wine.

## Tools

- `tools/install.py`: the installer (above).
- `tools/convert_x360_ui.py`: the 360's interface files (scheme, looks, imagesets, fonts, layouts,
  texts) converted from your 360 `pak.zip` and decrypted image into a folder for the PC game.
- `tools/stfs.py`: lists and extracts the files of an Xbox 360 package (CON, LIVE or PIRS).
- `tools/xex/xex_dump.py`: decrypts and decompresses the 360's `default.xex`.
- `tools/build_proxy.py`: builds the proxy `CEGUIBase.dll` (`proxy/`) from your game's own DLL.
- `tools/port_addresses.py`: GOG's addresses found in another build of `Torchlight.exe`, offline.
- `tools/pe_info.py`, `tools/compare_installs.py`, `tools/layout_windows.py`: reading the PC
  game's files.
- `tools/run_gog.sh`, `tools/steam_guard.py`, `tools/grab_window.py`, `tools/send_input.py`:
  development runs.
- `tools/check_no_game_files.py`: run by CI; fails if a game file is committed.

How the port is made, screen by screen, with the 360 functions it follows:
[`docs/xbox-ui-on-pc.md`](docs/xbox-ui-on-pc.md) (start at section 0).

## Building

The proxy is built with `clang-cl` and `lld-link` (LLVM) for 32-bit Windows:

```sh
python3 tools/build_proxy.py build "<game folder>/CEGUIBase_orig.dll" <output folder>
python3 tools/build_proxy.py install "<game folder>" <output folder>/CEGUIBase.dll
```

Tests use synthetic data only: `python3 -m unittest discover -s tests`.

## Credits

- Torchlight is by Runic Games.
- `tools/xex/lzx_decompress.py` is from [360tools](https://github.com/sp00nznet/360tools) (MIT).
- The prompt icons in `data/prompts` are Kenney's
  [Input Prompts](https://kenney.nl/assets/input-prompts) (CC0).

## License

GPL-3.0 ([LICENSE](LICENSE)), for the code. No file of either game is in this repository: the
converted interface is made from your own copies, on your machine. `tools/xex/lzx_decompress.py`
keeps its MIT license (`tools/xex/LICENSE-lzx_decompress`).

## Other versions

- **Humble Bundle:** not tested, as I do not have access to it. Its Windows version may work.
- **The native Linux version** (sold by Humble) is not compatible: the mod is for the Windows
  game. The installer does run on Linux, for the Windows game played through Proton or Wine.
