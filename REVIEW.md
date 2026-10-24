# Reviewing changes

How a review is done in this repository, for people and agents. The project's rules are in
`AGENTS.md`; this file says what counts as a failure and how findings are reported.

## What a failure looks like here

- Game files in git, from the 360 or PC: assets, executables, extracted data, captures, saves, run
  logs, or text copied from the game (a game text is named by its key and address, never quoted).
  A change that weakens `tools/check_no_game_files.py`.
- A change that can write, move or lose the player's saves or items: a save written while items
  are kept aside, an item destroyed or duplicated by a move cut short, a stop that lets the game
  save before it ends (Torchlight saves on closing, Alt+F4 too).
- A file the mod writes at run time put beside the game instead of `%LOCALAPPDATA%\x360ui`
  (Program Files and VirtualStore), beyond the documented exceptions (`x360_ui.ini`, which the
  installer writes; the developer dumps; the fallbacks when LOCALAPPDATA is missing or cannot be
  written: doc section 24, "The mod's own files out of the game's folder"), or an uninstaller that
  leaves the mod's folders behind or removes the player's.
- `tools/install.py` and `installer/src/install.cpp` doing different things: the C++ installer
  keeps the Python one's behaviour, and `tests/test_installer_cpp.py` checks it on synthetic games.
- Pad behaviour (moving, targeting, attacking, the 360's screens) that copies PC's mouse where the
  360 does something else, or a rule the 360 does not have, without the 360's function and the rule
  read cited in the doc.
- An address or offset used on a build where it was not verified (GOG's and Epic's executable,
  Steam's `proxy/addresses_steam.txt`), or a hook installed without the check that it matches.
- A text file read without the mod's one reader (the proxy's `x360_read_text`, the installer's
  `text_lines`), or one that breaks with CRLF, a lone CR or no final line end: the installer writes
  CRLF on Windows (`docs/regressions.md`, "The mod's texts read with CRLF"; `TextLfTest`).
- A claim of support that was not verified (README, docs, release notes, badges).
- A changed default or anything a player sees, without the owner's OK.
- A name or text taken from OpenTorchlight that is not marked *(OT)*, or its code copied.

## The reviewer's rules

- Every finding names `file:line` and the case that fails (inputs or state -> wrong result).
- Every finding is marked **Important** (must be fixed or justified before the merge), **Nit**
  (worth doing, does not block) or **Pre-existing** (not introduced by the change; noted for later).
- At most three nits. No findings about style.
- The summary starts with the number of findings, and says so plainly when there are none
  ("0 findings").
- A finding that comes up a second time goes into `AGENTS.md`, a test or a lint, not into another
  review comment.

## Sensitive areas

These always get a review in a clean context (a reviewer that has not written the change), and
the owner sees the evidence (test output, run logs) before the merge:

- the saves and the items kept aside (`proxy/x360_menu.c`'s stop and load hooks, the save hook);
- the user's data paths (`x360_data_path`, `x360_data_open`, the installers' `mod_data_folders`);
- the installers (`tools/install.py`, `installer/`);
- packaging and release (`packaging/`, `.github/workflows/release.yml`);
- CI workflows (`.github/workflows/`) and `tools/check_no_game_files.py`.

Documentation-only changes are integrated without a review in a clean context once CI is green.
