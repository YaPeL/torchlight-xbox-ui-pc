# The 360 interface in the PC game: feasibility (study, 2026-10-15)

This study was written in the recomp's repository (`torchlight-recomp`), where `docs/pc-ui.md`,
`docs/mods.md` and `game_menu/wide_layout.h` live; references to them below are to that repository.
No game file, from the 360 or from PC, goes into this one: only tools that read the user's own
copies (`tools/`). PC was not run for this study; everything is read from the user's files (PC's
`Pak.zip`, `Torchlight.exe` and its DLLs, the Xbox pak and the decrypted Xbox image). Marks:
**[read]** (read in the files), **[inferred]**, **[to verify]**.

## 0. Start here (handover, 2026-10-15)

This repository is the whole of this project; the recomp's repository is only a reference.

**Rules from the user, standing:**
- No game file in the repository (CI fails on one: `tools/check_no_game_files.py`); tools convert
  from the user's own copies, into folders outside the repository.
- Tests use synthetic data only.
- The repository is private and has no license until the user decides.
- Work on the GOG version. Steam is tried last, with section 4's full protocol.
- Never use Steam's prefix (`compatdata/41500`) and never touch the user's saves. Before any PC
  run, take a hash snapshot; after it, verify the snapshot.
- Ask before running the game when the user has not approved that kind of run. When a run needs
  them, give the steps and stop.
- Docs, commits and code are in English. Replies to the user are in Spanish (voseo).
- If two hypotheses in a row fail on one point, stop, write down what was found, and report.

**Where things are on this machine:**

| What | Where |
|------|-------|
| GOG offline installer (the user's) | `~/<the GOG offline installer, 1.15a build 23675>.exe` |
| Its files, extracted with `innoextract -e` (nothing run) | `~/torchlight-gog/extract` |
| The game folder used | `~/torchlight-gog/prefix/drive_c/GOG Games/Torchlight` (a copy of the extract, without `tmp/`, `__redist/`, `commonappdata/`) |
| The working prefix (Proton's script) | `~/torchlight-gog/compat/pfx` |
| GOG saves, settings, mods, `Ogre.log` | `~/torchlight-gog/compat/pfx/drive_c/users/steamuser/AppData/Roaming/runic games/torchlight/` (lower case on disk; `save/`, `mods/`, `settings.txt`, `local_settings.txt`, `resourceconfig.dat`) |
| Proton's logs of our runs | `~/torchlight-gog/steam-gogtl.log` |
| The converter's output (stage 0) | `~/torchlight-gog/x360ui` (`media/ui/x360/`) |
| The proxy's build | `~/torchlight-gog/build/proxy` (`CEGUIBase.dll`, the generated `.def` files) |
| The proxy, installed | the game folder: `CEGUIBase.dll` (proxy), `CEGUIBase_orig.dll` (real), `x360_ui.ini`; its log `x360_ui.log` in the prefix's `AppData/Local/x360ui` |
| Steam snapshot (hashes) | `~/torchlight-gog/hashes/steam-guard.json` (2404 entries, 2026-10-15) |
| Steam's Torchlight install (read only, for comparisons) | `~/.steam/debian-installation/steamapps/common/Torchlight` |
| The Xbox game data (read only) | `~/360tools/extracted/extracted` (`pak.zip`) |
| The first prefix (wine alone; it does not work, now only holds the game folder) | `~/torchlight-gog/prefix` |

**Running GOG, step by step:**
1. Snapshot Steam:
   `tools/steam_guard.py snapshot ~/torchlight-gog/hashes/steam-guard.json ~/.steam/debian-installation/steamapps/compatdata/41500 ~/.steam/debian-installation/userdata/<steam-id>/41500`.
   If a snapshot was already taken, check that it still holds with `verify` before overwriting it.
2. Run `tools/run_gog.sh`. First it backs up the saves the run will use with
   `tools/save_guard.py backup` (the folder found from the prefix's own `user.reg`, copied to
   `~/torchlight-gog/save-backups/run-<time>` and checked by hash) and stops if that fails. Then it
   calls `proton run` from Proton Experimental with:
   - `STEAM_COMPAT_DATA_PATH=~/torchlight-gog/compat`;
   - `STEAM_COMPAT_CLIENT_INSTALL_PATH` set to Steam's folder, only so the script finds it;
   - `SteamGameId=gogtl` and `SteamAppId=0`, both needed: without them the script exits at once
     with code 1 and no log;
   - `WINEDLLOVERRIDES="winemenubuilder.exe=d"`, so no shortcuts are made;
   - `PROTON_LOG=1`.

   Steam need not be open: GOG's executable does not import `steam_api`, so "Failed to connect to
   Steam" in the log is harmless.
   With the proxy installed (section 7), `x360_ui.log` (in the prefix's `AppData/Local/x360ui`) lists every scheme and
   layout the game asks for (and, with `trace=1`, section 8). `tools/grab_window.py Torchlight
   OUT.png` saves a screenshot; `tools/send_input.py` clicks and types (section 8).
3. The game opens at 1920x1080 (the desktop's size; `local_settings.txt` has `WINDOW_WIDTH`,
   `RES_WIDTH`, `FULLSCREEN`) with DXVK, and reaches the main menu. Close it when done: Alt+F4 or
   the game's own Quit, or, from a script, Proton's `files/bin/wineserver -k` with
   `WINEPREFIX=~/torchlight-gog/compat/pfx` (it stops that prefix's processes only).
4. Run `tools/steam_guard.py verify ~/torchlight-gog/hashes/steam-guard.json`. It must print
   UNCHANGED. Also check that nothing new appeared on the desktop (`~/Escritorio`) or in
   `~/.local/share/applications`.

   The only file of Steam's tree a run touches is Proton's own `dist.lock`, the lock file the
   script opens on every run. It is outside the guarded folders.

**What does not work yet:**
- **Sound:** "No driver from pulse,alsa,oss,coreaudio", because the unix side of `winepulse`
  cannot be loaded. Proton normally runs inside Steam's runtime container
  (`SteamLinuxRuntime_sniper`), which provides the PulseAudio/PipeWire client libraries its unix
  modules link against. Likely fixes, untried: run through the runtime's `_v2-entry-point` as Steam
  does, or with the host's `libpulse` visible to Proton's `winepulse.so`. Not needed for the UI
  work.
- **Wine alone (no Proton script):** it stops before the menu (section 5a), so do not use it.

**The tools** (`tools/`; tests in `tests/test_tools.py`, synthetic only, run by CI):

| Tool | What it does |
|------|--------------|
| `pe_info.py` | Sections, imports and exports of an `.exe`/`.dll` |
| `compare_installs.py` | Two installs' binaries compared: hash, link time, sections, imports, exports |
| `port_addresses.py` | GOG's addresses found in another `Torchlight.exe` build by independent ways (section 16) |
| `compare_offsets.py` | The struct offsets of two builds' copies of the same functions compared (section 16) |
| `compare_ui.py` | A 360 catalog screen against PC's dump of the same screen, window by window (section 14) |
| `layout_windows.py` | The named windows of a CEGUI layout, and how many of them another layout has |
| `convert_x360_ui.py` | Stage 0: the 360 UI files from the user's `pak.zip` into `OUT/media/ui/x360/` (section 6) |
| `build_proxy.py` | Stage 1: builds the proxy `CEGUIBase.dll` from the user's DLL (clang-cl, lld-link), for GOG's build or, with `--target steam`, Steam's (section 16); installs and removes it in a game folder |
| `grab_window.py` | A screenshot of the game's window (X11 under Xwayland) |
| `send_input.py` | Mouse and keyboard input for the game, sent from inside its prefix (`send_input.exe`, built by `build_proxy.py`) |
| `steam_guard.py` | Hash snapshot and verify of folders that must not change |
| `save_guard.py` | The save folder a GOG run will use, backed up and checked before the run |
| `pad_trace.py` | The movement's measurement: the recorded stick routes (`routes`), a trace's numbers (`summary`), and a trace against the recompiled 360's (`compare`, with an SVG plot) |
| `run_gog.sh` | The GOG run above (shell, no test) |
| `check_no_game_files.py` | The CI check |

Run the tests with `python3 -m unittest discover -s tests -v`.

**State:**
- The feasibility study is done (sections 1 to 3).
- The GOG/Steam comparison is done (section 5): two builds, the same UI data except
  `charactercreate.layout`, and the same CEGUI imports but for one function. Work done against
  GOG carries over to Steam where it goes through CEGUI and the data, not where it hooks addresses
  of the executable.
- GOG runs (section 5a).
- Stage 0, the converter, is done (section 6): `tools/convert_x360_ui.py`, tested on synthetic
  paks and run on the user's 360 pak.
- Stage 1, the proxy `CEGUIBase.dll`, is done (section 7): it logs what PC loads, and with
  `mode=scheme` adds the converted folder to Ogre and loads the 360 scheme after PC's. The game
  reaches its main menu with it, unchanged. No 360 layout is shown yet.

**Gameplay with the pad, state (2026-10-15):** the user's verdict on the last run: "todo ok".
Working with the real pad (section 9e): moving and stopping, X (the 360's target rule, the
highlight, attacks in place, breaking barrels), A (picking up items, doors, chests, the shared
stash, merchants, NPCs with "!" and "?", walking NPCs, portals), the hotbar keys (LB drank a
potion). Open: the hero spinning in place when the stick spins fast (the 360 turns in place; the
turn rate is the same, so it is when the hero starts walking), the 360's item name window on the
highlighted item, the skills on B/Y/LT/RT not tried with assigned skills. Next: the inventory
slice (section 10c).

**Focus (2026-10-15, the user's order):** the core of the project is gameplay with the pad and the
360's screens with logic of their own (inventory, character, skills, pet, merchant, enchanter);
the `onClick` menus are the cheap part, done last or when a test needs them. Section 9 holds the
gameplay study, in progress.

**Paused (2026-10-15, the user's order):** stage 2 (the modal and the small screens) is paused
where section 8 leaves it, on branch `stage2-modal`; the work now is gameplay with the pad
(section 9).

**Next, in order (section 3's stages):**
1. **Stage 2, the small shared screens with the 360 look** (waypoints, quest dialog, modal,
   death, character load): in the proxy's `loadWindowLayout`, hand the game the converted 360
   layout instead of PC's, with the windows PC's code expects added to it (section 2's counts).
   The first one should be the one closest to PC's: the waypoints (21 of 25) or the modal box
   (5 of 6). A check that the 360 scheme's imagesets and fonts really loaded comes with it (the
   first 360 layout drawn), since this build writes no CEGUI log.
2. Stages 3 to 5 as in section 3.

## 1. What the 360 interface is made of

- **48 layouts** (`media/UI/*.uilayout`), CEGUI XML (`GUILayout`) like PC's `.layout`, with the
  `ceguiWidget/*` window types of `media/ui/hud/schemes/ceguiWidgets.scheme` and its
  `ceguiWidgets.looknfeel`. The scheme maps every type onto standard CEGUI classes and Falagard
  renderers (no Runic window class), as PC's `GuiLookSkin.scheme` does. **[read]**
- **Imagesets and textures:** `ceguiWidget.imageset`, `icons2/widgets*.imageset`,
  `ratings.imageset`, `loading.imageset` and the screens' own; authored at 1024x768 with
  `AutoScaled`, as PC's; the textures are ordinary little-endian DDS (DXT3 for the widgets), which
  PC's renderer reads as they are. **[read]**
- **Fonts:** the same names and TTF files as PC's (`Serif*`, `FrizQuadrata*`), plus `Serif14` and
  `FrizQuadrataSmall/NoScale`. **[read]**
- **Item icons:** the Xbox `itemicons13/14/16` atlases differ from PC's (other image counts and
  places, `docs/pc-ui.md` 1b): a 360 screen in PC must keep using PC's icon imagesets, which the
  PC game's item data points at. **[read]**

## 2. What PC would have to run them

- **CEGUI on PC is a DLL** (`CEGUIBase.dll`, `CEGUIFalagardWRBase.dll`, `CEGUIExpatParser.dll`,
  `OgreGUIRenderer.dll`; CEGUI 0.6 era, with OGRE 1.6 **[inferred]**). `Torchlight.exe` imports 175
  functions from `CEGUIBase.dll`, `WindowManager::loadWindowLayout` among them, and names its scheme
  (`media/ui/guilookskin.scheme`, `windowslook.scheme`) and its layouts (`media/ui/pethud.layout`,
  ...) as strings. **[read]** A proxy `CEGUIBase.dll` (same exports, forwarding to the real one) is
  therefore a clean place to load the 360 scheme after PC's and to hand the game a 360 layout when
  it asks for a PC one, without patching `Torchlight.exe`. The executable has no packer or Steam
  DRM section (`.text`, `.rdata`, `.data`, `.rsrc`, `.reloc`). **[read]**
- **The menus' code:** PC's classes find PC's window names. Of the windows PC's code expects (its
  layout's named windows), the 360 layout of the same screen has: waypoints 21 of 25, quest dialog
  18 of 20, character load 24 of 41, death 7 of 9, modal 5 of 6, tip 2 of 7, pet HUD 6 of 15,
  settings 8 of 26; inventory 5 of 98, pet menu 3 of 95, stats 0 of 52, quests 1 of 35, journal
  0 of 7. **[read]** The 360's gameplay screens are `...Xenon` classes (`CInventoryMenuXenon`,
  `CCharacterMenuXenon`, `CCharSkillsMenuXenon`, `CStashMenuXenon`, `CMerchantMenuXenon`,
  `CEnchantMenuXenon`, `CQuestMenuXenon`, `CStatsMenuXenon`, `CSettingsMenuXenon`,
  `CGameSaveMenuXenon`, `CHowToMenu`, ...) that PC does not have. **[read: RTTI of both]** So, as in
  the other direction, only the small shared screens can take the other platform's layout; the
  360's inventory, character sheet and skill screens would have to be written again for PC.
- **The pad:** PC has none. No XInput, and `OIS.dll` ships but `Torchlight.exe` does not import
  it; input is Win32 (`GetAsyncKeyState`, `GetCursorPos`, `SetCursorPos`, `ShowCursor`) fed to
  CEGUI's `inject*`. **[read]** The 360's focus movement lives in Xbox-only classes
  (`CDropdownMenuTabNavigationHandler`, `CSubMenuTabNavigationHandler`) **[read]**, so on PC the pad
  needs: XInput read by the proxy (or a `xinput1_3.dll`/`dinput8.dll` proxy), a focus model per
  screen written anew, and the pad's buttons turned into PC's commands (`GUIACCEPT`, `GUIBACK`,
  `GUITOGGLEINVENTORY`, ...) or keys. Moving and fighting with the stick is a separate, larger
  piece: PC's character moves by mouse clicks on the ground.
- **Resolution:** the 360 layouts are drawn for a 16:9 1280x720 frame with a safe area; PC runs any
  resolution with its 1024x768-based layouts. The 360 layouts at 4:3 or 16:10 need the band
  re-anchoring the recomp already does the other way (`game_menu/wide_layout.h`).

## 3. Plan by stages and cost

| Stage | What | Cost | Needs PC to run |
|-------|------|------|-----------------|
| 0 | A converter from the user's 360 copy: the UI files (`.uilayout` as `.layout`, the scheme, looknfeel, imagesets, widget textures, the extra fonts) into a folder PC can read (a PC mod folder or the replacements-style folder of the new project); nothing of the game in the repository | 1-2 days | no |
| 1 | The proxy `CEGUIBase.dll`: forward every export; after PC loads `guilookskin.scheme`, load the 360 scheme; log the layouts PC asks for | 2-3 days (x86, MSVC 2008 ABI: the exports are C++ members, forwarded by ordinal/name) | yes |
| 2 | The small shared screens with the 360 look: waypoints, quest dialog, modal, death, character load; the windows PC's code expects added to the converted layouts | 1-2 days each | yes |
| 3 | The pad on PC: XInput read by the proxy, a focus model for the shared screens, the buttons as PC commands | 1-2 weeks | yes |
| 4 | The 360's gameplay screens (inventory, character, skills, stash, merchant, enchant): written for PC against PC's game data, from the proxy | weeks per screen family | yes |
| 5 | Playing with the pad (stick movement, targeting, the 360's skill wheel): hooks into `Torchlight.exe` itself | a project of its own | yes |

Stages 0-2 are a few weeks and give the 360 look on PC's small screens; the 360 experience as a
whole (pad play and its screens) is stages 3-5, far larger, and mostly new code on PC.

## 4. Running the PC game: precautions

The work is done on the GOG version, in a prefix of its own (section 5); the Steam version is
tried last, with the precautions below.

Nothing in the study needed PC to run; stage 1 onwards does. For GOG, section 0's snapshot and
verify are the check. For Steam, before any run:

1. **Copies with hashes:** the Proton prefix used, and Steam's `userdata/<id>/41500/` (the cloud
   folder), copied aside; a SHA-256 list of every file in both, kept with the copy.
2. **A separate prefix**, never `compatdata/41500`, which holds the user's saves and settings: a
   new prefix for the tests, the game folder copied into it (or a copy of the game folder), so the
   proxy DLL never lands in the user's install.
3. **Steam closed and unable to relaunch the game:** with Steam closed, the game asks Steam to
   start it (`steam://run/...`), which reopens it in the real prefix (seen in the 2026-10-14
   benchmark). The usual guard is a `steam_appid.txt` next to the copy's executable, with which
   Steamworks does not restart the app **[to verify with this build, in the separate prefix]**; and
   no Steam process left before each run (checked, not assumed), with the `steam` command not
   reachable from the test's environment.
4. **Steam Cloud off** for the game, so nothing the test writes is uploaded.
5. **After each session:** the hash lists checked against the copies, the real prefix and
   `userdata/<id>/41500/` unchanged; anything changed is restored from the copy and reported.

## 5. The GOG version, and what of it holds for Steam (2026-10-15)

Work is done on the GOG version (offline installer `setup_torchlight_1.15(a)_(23675).exe`, game
ID 1207659069), read with `innoextract` and compared with the Steam install by
`tools/compare_installs.py` and the paks' file lists, without running either. **[read]**

- **Two builds.** GOG is build 17870 (`BuildVer.txt`, 2010-08-30; `Torchlight.exe` linked again
  in 2012), Steam is build 17685 (2010-01-26). The executables differ in code (`.text` 6.83 MB
  against 6.66 MB) and Steam's has a `.reloc` section GOG's lacks. GOG's imports no
  `steam_api.dll` at all (Steam's: `SteamAPI_Init`, `SteamAPI_RestartApp`, `SteamRemoteStorage`,
  `SteamUserStats`, ...), so GOG cannot relaunch itself through Steam. Steam's imports
  `CopyFileW`/`MoveFileW`/`SetFileTime`/`ShellExecuteW`; GOG's imports `CreateProcessW`.
- **DLLs.** `CEGUIBase.dll`, `OgreMain.dll` and `ParticleUniverse.dll` are different builds too
  (GOG's newer: CEGUIBase 2010-03 against 2009-11; GOG's CEGUIBase has a few more wide-string
  imports). The rest (`CEGUIExpatParser`, `CEGUIFalagardWRBase`, `OgreGUIRenderer`, `OIS`,
  `fmodex`, `cg`, the render systems and plugins, `ReferenceAppLayer`) differ only in the PE
  header checksum and the Authenticode signature: the same code. `d3dx9_39`, `msvcp90` and
  `msvcr90` are identical. Torchlight.exe's imports from `CEGUIBase.dll` differ by one function each
  way (GOG: `String::assign(const char*)`; Steam: `UVector2(UDim, UDim)`).
- **Data.** GOG's `Pak.zip` has 16 more files and 3611 that differ, nearly all unit data
  (3466 under `media/units`, the balance changes of the later build). Of the UI only
  `media/ui/charactercreate.layout` and the menu models' animations differ: every other layout,
  imageset and scheme is the same file.
- **Saves and mods.** Both keep them under `%APPDATA%\Runic Games\Torchlight\` (`save`, `Mods`,
  `settings.txt`, `local_settings.txt`; the GOG script's save folder is
  `{userappdata}/runic games/torchlight`).

**What holds for Steam:** the approach (a proxy `CEGUIBase.dll`, the converted layouts, the
scheme), since both executables load the same kind of CEGUI through the same imports, and every
converted UI file but the character creation layout. **What does not carry over as it is:**
anything bound to an address in `Torchlight.exe`, `CEGUIBase.dll` or `OgreMain.dll` (stage 5's
hooks, or a proxy that patches code), which must be found again in Steam's build, and the proxy
must forward the CEGUIBase exports by name (the two builds' export tables are not known to match
by ordinal). **[inferred]** Steam is tried last, with section 4's precautions.

### 5a. Running GOG (approved by the user 2026-10-15)

- **Prefix:** a new one, `~/torchlight-gog/prefix`, made by the wine of the user's Proton
  (`Proton - Experimental/files/bin/wine` with that prefix), never Steam's `compatdata/41500`.
  No Steam client, no Proton launcher: the wine binary alone.
- **Install:** the game folder extracted by `innoextract` (no installer code run, no GOG Galaxy,
  no cloud: the offline installer has neither) copied to the prefix's `C:\GOG Games\Torchlight`.
- **Saves:** inside the prefix, `drive_c/users/<user>/AppData/Roaming/Runic Games/Torchlight/`;
  nothing outside `~/torchlight-gog`. The Steam saves are not read or written.
- **What runs:** `wineboot` once to create the prefix, then `Torchlight.exe` from the copy, with
  `WINEDLLOVERRIDES="winemenubuilder.exe=d;mscoree=d;mshtml=d"` (no shortcuts or file associations
  on the desktop; no Mono or Gecko install prompts).
- **Before and after:** SHA-256 of every file of Steam's `compatdata/41500` and
  `userdata/<id>/41500` (758 files), and their tree, taken before the prefix was made and checked
  after the runs: unchanged. Nothing in the Proton folder was modified (no file newer than the
  start), no desktop entry was made.

**What was seen (2026-10-15).**
- The prefix's user is `steamuser` (Proton's wine), and the game writes to
  `drive_c/users/steamuser/AppData/Roaming/Runic Games/Torchlight/`: `Ogre.log`, `settings.txt`,
  `local_settings.txt`, `mods/`, `save/`, `torched/`. The game's own log says "Path for saving is
  ... C:/users/steamuser/AppData/Roaming/Runic Games/Torchlight/SAVE". **[read: the run]**
- Proton's wine run alone lacks what Proton's launcher puts in a prefix. First,
  `RenderSystem_Direct3D9.dll` did not load because wined3d needs `libvkd3d-shader-1.dll` and
  `libvkd3d-utils-1.dll`. Copied from Proton's `lib/vkd3d/` into the prefix (read only from Proton),
  the render system loaded. Sound has no driver: the unix side of `winepulse` is not found, likely
  Steam's runtime libraries.
- Then the game starts its 800x600 window but computes a window size of 6352240 x 0
  (`CreateViewports AspectRatio message - 6.35224e+006 x 0`) and stops. With wined3d it was a page
  fault at 0x0152B6B8, after GL errors on a 0-sized texture. With DXVK's `d3d9.dll` (copied from
  Proton into the prefix's `syswow64`, `d3d9=n`) it was "Failed to create Direct3D9 Device"
  (DXVK: cannot create a 6352240x1 X8R8G8B8 texture). It did not reach the main menu. **[read: the
  runs]**
- **Step 0 (the user's order: the error first).** With `WINEDEBUG=+loaddll,err` and wine's builtin
  `d3d9`/`wined3d`, the crash is a page fault in `CEGUIBase.dll` +0x7b6b8 (`flds 0x1c(%eax)`, a
  null pointer), after `CreateViewports` measured the window as 6352240 x 0. The game's only log
  is `Ogre.log` in the AppData folder above; there is none next to the executable. The extracted
  build is the base installer 1.15(a), build 17870, not GOG's 1.0.1 update, which is reported to
  fail under Wine.
- **Step 1: Proton's own script works.** The run was `proton run` from Proton Experimental:
  - `STEAM_COMPAT_DATA_PATH=~/torchlight-gog/compat` (a new prefix, made by the script);
  - `STEAM_COMPAT_CLIENT_INSTALL_PATH` set to Steam's folder, only for the script to find it;
  - `SteamGameId=gogtl` and `SteamAppId=0`: without an id the script stopped at once with code 1
    and wrote no log;
  - `WINEDLLOVERRIDES="winemenubuilder.exe=d"` and `PROTON_LOG=1` into `~/torchlight-gog`.

  The game opened at 1920x1080 with DXVK (`CreateViewports AspectRatio message - 1920 x 1080`) and
  reached the main menu (seen by the user, who closed it). "Failed to connect to Steam" in the log
  is harmless: GOG's executable does not use Steam. Sound has no driver here either.
- **Saves of the working setup:**
  `~/torchlight-gog/compat/pfx/drive_c/users/steamuser/AppData/Roaming/Runic Games/Torchlight/`
  (`save/`, `mods/`, `settings.txt`, `local_settings.txt`, `resourceconfig.dat`, `Ogre.log`).
  The game folder stays at `~/torchlight-gog/prefix/drive_c/GOG Games/Torchlight` and is run
  through `Z:`.
- **Checks after step 1:**
  - Steam's `compatdata/41500` and `userdata/<id>/41500` are unchanged (hashes and tree).
  - No desktop or menu entry was made.
  - In Proton's folder only `dist.lock` changed: the lock file the script opens on every run, as
    it does when Steam starts a game.

  The first prefix (`~/torchlight-gog/prefix`, wine alone) is now used only as the game's folder.

## 6. Stage 0: the converter (2026-10-15)

`tools/convert_x360_ui.py PAK OUT` reads the user's 360 `pak.zip` and writes
`OUT/media/ui/x360/` (flat) and `OUT/x360_ui_report.json`. It refuses an `OUT` inside this
repository. Run on the user's pak: 71 files, 48 layouts, 6 imagesets, 7 fonts. **[read: the run]**

**What it takes.** Driven by the 360 scheme (`media/ui/hud/schemes/ceguiWidgets.scheme`):
- the scheme and its looknfeel;
- the 6 imagesets the scheme names (`ceguiWidget`, `Loading`, `widgets`, `widgets1`, `widgets2`,
  `ratings`) and their DDS textures (DXT3, `loading.dds` DXT1; little-endian, as PC's);
- the fonts it names (`Serif14.font` once, though the scheme names it twice) and their TTFs;
- every `.uilayout` (48), written as `.layout` under its own name.

It does not take the 360 pak's `.layout` files (PC's own, and `*_xenon.layout` versions of the
360 screens; the 360 game's are the `.uilayout` ones **[inferred: the recomp's docs/mods.md]**),
the item icon atlases (section 1: PC's must stay), or the screens' own imagesets (the class
intros, `vignettebackdrop`, `deadordraake`, `WindowsLook`: the same files as PC's). **[read]**

**What it changes.** Only names and paths; every converted layout equals its `.uilayout` once
`set:X360_` is read as `set:`, and every output XML file parses. **[read]**
- Every path becomes `media/ui/x360/<file>`, so no path of PC's UI is replaced.
- Every imageset of the scheme becomes `X360_<name>` in the imagesets, the scheme, the looknfeel
  (`imageset="..."`, `set:...`) and the layouts. Both games have an imageset `Loading`, on
  different `loading.dds` textures, and CEGUI keeps the first imageset of a name.
- The scheme becomes `X360_newHud`. Window types keep their names (`ceguiWidget/*`; PC's are
  `GuiLook/*` and `WindowsLook/*`).
- Font names are kept. The 360's `Serif`, `SerifBig`, `SerifHuge`, `SerifSmall`, `FrizQuadrata`,
  `FrizQuadrataBig` and the three TTFs are the same files as PC's, so whichever loads first draws
  the same; `Serif14` is the 360's only new font, and its layouts use it 234 times. **[read]**

**What PC's CEGUI will meet.** The layouts use 10 window types, all mapped by the scheme onto
standard CEGUI classes and Falagard renderers. Their properties are standard CEGUI 0.6 ones
(`WantsMultiClickEvents`, `VerticalAlignment`, `UnifiedMinSize` are strings of PC's
`CEGUIBase.dll`) or are defined by the 360 looknfeel itself (`tabOrder`, `DisabledImage`). **[read]**

**Left as they are, and reported.** 8 layouts name imagesets that are in neither the scheme nor
the pak: `widgets3` to `widgets6` and `characters` (`charpetstatsmenu_xenon`,
`charskillsmenu_xenon`, `controls_tray`, `skills_controls_tray`, `stats_controls_tray`,
`equipmenu_xenon`, `playerpetstats_journal_menu_xenon`, `skillsmenu_xenon`), and
`equipmenu_xenon` an editor image (`set:1024X768.PNG`). Those windows will draw no image on PC,
as on the 360 if it loads them at all. **[read]**

## 7. Stage 1: the proxy CEGUIBase.dll (2026-10-15)

**The DLL.** `proxy/cegui_proxy.c`, built by `tools/build_proxy.py build [REAL_DLL] BUILD_DIR` with
`clang-cl` and `lld-link` 21 (`--target=i686-pc-windows-msvc`, freestanding, no CRT; import
libraries for `kernel32` and `msvcr90` made by `llvm-dlltool` from generated `.def` files). Its
export table was generated from the user's own DLL at build time (since 2026-10-23 it is read from
`proxy/cegui_exports.txt`, names and ordinals only: section 18): each of the
3839 exports is forwarded by name, with its own ordinal, to `CEGUIBase_orig.dll` (the real DLL,
renamed), except three, which go to `__fastcall` thunks (the `__thiscall` convention with a
dummy `edx`; they pop 8, 8 and 20 bytes, checked in the disassembly):
`SchemeManager::loadScheme(const String&, const String&)` and the two
`WindowManager::loadWindowLayout`. The build also checks that every function the source looks
up at run time is exported by the game folder's DLLs (a typo in a mangled name cost one run).
`build_proxy.py install|remove|status GAME_DIR` swaps the DLLs and refuses to overwrite a real
DLL. **[read: the build]**

**All 32-bit.** `Torchlight.exe`, `CEGUIBase.dll`, `OgreMain.dll`, `OgreGUIRenderer.dll` and the
proxy are i386 (machine 0x14c). **[read]**

**One DLL for GOG and Steam.** GOG's and Steam's `CEGUIBase.dll` export the same 3839 names with
the same ordinals (no export by ordinal only), and the proxy built from GOG's has exactly that
table. Every module that imports `CEGUIBase.dll` (`Torchlight.exe` 175 functions,
`CEGUIFalagardWRBase` 195, `OgreGUIRenderer` 61, `CEGUIExpatParser` 22) imports by name. So the
same proxy serves both. The functions the proxy looks up at run time (in `CEGUIBase`,
`OgreMain`, `msvcp90`) are all exported by Steam's DLLs too, by the build's name check run on
Steam's folder, read only. **[read; no Steam run done]**

**Log mode (no `x360_ui.ini`): the game is unchanged.** The game reached its main menu as it
did without the proxy: the screenshots' UI band (the bottom 120 rows at 1920x1080) is identical
pixel for pixel, the rest differs only in the animated 3D scene. **[read: the runs]** What PC
loads, in order, before the main menu (`x360_ui.log`; none returned null):
- schemes `MEDIA/UI/GUILOOKSKIN.SCHEME`, then `MEDIA/UI/WINDOWSLOOK.SCHEME`, both with an empty
  resource group (paths upper-cased by the game);
- 37 layouts: `LOADING`, `BOTTOMHUD`, `PETHUD`, `INVENTORYMENU`, `SKILLMENU`, `JOURNALMENU`,
  `QUESTMENU`, `MERCHANTMENU`, `ENCHANTMENU`, `COMBINEMENU`, `STASHMENU`, `STATSMENU`, `PETMENU`,
  `OPTIONSMENU`, `SETTINGSMENU`, `DIEMENU`, `WAYPOINTMENU`, `DIALOGMENU`, `QUESTDIALOGMENU`,
  `CINEMATICMENU`, `MODALMENU`, `TIPMENU`, `INTERACTIVEMENU`, `FISHINGMENU`, `CONSOLE`,
  `MAINMENUFRAME`, `CHARACTERCREATE`, `CHARACTERLOAD`, `DIFFICULTY` through the two-argument
  `loadWindowLayout` (random prefix); `SKILLTOOLTIP` (4 times), `TOOLTIP`, `TOOLTIPCOMPARE`
  (twice) and `SKILLFOLDOUT` through the five-argument one, with prefixes `gui__NNNN`. Every
  screen's layout is loaded once at start, not when the screen opens: replacing a layout means
  replacing it at this point.

**Scheme mode.** `x360_ui.ini` in the game folder:
`mode=scheme` and `out=Z:\home\<user>\torchlight-gog\x360ui` (the converter's `OUT`). After
the game's `loadScheme` of `GUILOOKSKIN.SCHEME` returns, the proxy:
1. adds `OUT` to Ogre's world resource group (`General`) as a recursive `FileSystem` location.
   `OUT`, not `OUT/media/ui/x360`, because the converted files name each other by their own
   paths (`media/ui/x360/...`); the folder added holds only those files and the report. Ogre's
   `std::string` arguments are built and destroyed by `msvcp90.dll`'s own exported members;
2. loads `media/ui/x360/ceguiWidgets.scheme` with the real `loadScheme`, inside `__try` (an
   exception there is logged and the game's call returns as it would have).

Run (2026-10-15): the location was added, `resourceExists` found the scheme, and the 360 scheme
loaded in 44 ms (`loadScheme (360)` returned a scheme, so its imagesets, fonts and looknfeel
were all found **[inferred: CEGUI 0.6 throws on a missing file]**). Then `WINDOWSLOOK.SCHEME` and the 37 layouts
loaded as before and the game reached the same main menu, its UI band again identical to the
run without the proxy. **[read: the run]**

**`mods/` (a data point only).** With a file at a new path in a mod folder
(`mods/X360Probe/media/ui/x360probe/probe.txt`), `resourceExists` in `General` said no when
PC's scheme had just loaded. So by then the mod folder's new paths are not in the group the UI
reads from. **[read: the run]** It is not known whether the game adds mods later or to another
group. With a mod folder present the start took about 20 s longer (the mod scan); `mods.dat` was
not changed. The probe was removed after the run.

## 8. Stage 2, first findings: what PC's code asks of its windows (2026-10-15)

*Paused here (2026-10-15): the modal is not converted yet. Left open: how to see PC's modal in
play (it is shown only from the death handler), and the mapping data files.*

**The trace (`trace=1` in `x360_ui.ini`).** The proxy also intercepts, for calls from
`Torchlight.exe` only (by return address), `Window::getChild(const String&)`,
`Window::recursiveChildSearch`, `PropertySet::getProperty`, `Window::setText` and
`System::injectKeyDown/KeyUp/Char`, and logs each distinct case once. A full run (start, a new
character, the town, the options menu, back to the title, the load screen) is kept outside the
repository (`~/torchlight-gog/x360_ui-trace-pc.log`). **[read: the run]**
- The game finds windows by their unprefixed names (`Accept`), though in the tree they are
  prefixed (`20_Accept`, the layouts' random prefixes being a counter): Runic's CEGUI compares
  names without the prefix. It does not use `WindowManager::getWindow` at all.
- After each layout loads, the game reads `onClick` of every window that has the property (a
  Falagard property of the look) and binds the click to that command (`guiAccept`, `guiOk`,
  `guiSelect1`, ...).
- The modal (`MODALMENU.LAYOUT`, loaded at start, prefix `20_`): `recursiveChildSearch` of
  `Decline`, `Accept`, `Ok`, `Dialog`, `Title` from the root; `onClick` read on `Decline`,
  `Accept`, `Ok` and the root `Frame` (empty); `setText` of `Title` (`"Dialog"`). Its class keeps
  `Ok`, `Accept`, `Decline` at +0x6c, +0x70, +0x74 (`CModalMenu`, vtable 0xab7d04, constructor
  0x590d40, init 0x590860). **[read: the trace and the disassembly, GOG build]**
- The tip (`TIPMENU.LAYOUT`, `21_`): `Ok`, `Dialog`, `Title`, `ShowTips`; `onClick` of `Ok`; its
  text is set when it opens (the town's first tip at a new game). Enter closes it (the game's own
  key handling: CEGUI does not consume Enter).

**What this means for the 360 modal.** Its `Accept`, `Ok` and `Decline` are `DefaultWindow`
containers (a pad button icon and a label), with no `onClick` property: PC's code would find
them by name but bind no click. Mapping them means giving the windows of those names a type
whose look defines `onClick` and takes the mouse (or turning the label into one), with the icon
and label passing the mouse through. **[inferred]**

**When PC shows the modal.** `CModalMenu`'s show method (0x590bf0, `ret 0x10`) has one caller
(0x54cb69, a UI wrapper at 0x54ca50), which has one caller: 0x4e16c5, in a function of the
player's death handling (it goes on to `DESTROY_ON_DEATH`), after a check of a flag (`0xab`).
**[read: the disassembly; that it is the hardcore death inferred]** The confirmations met in
play are other screens' own windows: deleting a character is `characterload.layout`'s
`DeleteConfirm`; "Exit to Title" and "Quit Game" ask nothing. So the modal is seen in play only
at that death. *(Corrected 2026-10-21: the fishing's result is this modal too, "Fishing" with
Ok, `screens/gog/fishing-catch.png`; its show path there was not traced.)*

**Input and screenshots under Xwayland.**
- X11's XTest does not move the pointer in this Xwayland (24.1; GNOME). `send_input.exe`
  (`proxy/send_input.c`, x86, no CRT) runs inside the GOG prefix with Proton's wine binary (it
  joins the game's wineserver) and calls `SendInput`/`SetCursorPos`. Mouse and keyboard both
  reach the game: the clicks press its buttons, the keys reach `injectKeyDown`/`injectChar`.
- When the game loses the focus, Wine minimizes its window (for Wine; X still shows it), and
  input does not reach it; `send_input.exe` restores it first.
- After that restore the game draws into an untitled X window of the same size, and the titled
  one keeps a stale frame; `grab_window.py` grabs the untitled one when there is one. A screen
  can also take a few seconds to change after a click (the title to the load screen: about 3 s).

## 9. Gameplay with the pad: the study (2026-10-15)

Sources: the 360 image (`guest-image.bin`) and the recompiled code of `~/torchlight-recomp`, read
only (its `tools/guest_re` scripts run with `python3 -B`, writing nothing there); GOG's
`Torchlight.exe` disassembled (`llvm-objdump`); GOG runs. Function names marked *(OT)* come from
OpenTorchlight (github.com/Qweasd123tg/OpenTorchlight, a decompilation of the Linux build), its
symbol list `research/original-symbols.txt` and call graph `research/original-callgraph.tsv`,
read only, cloned outside this repository (`~/ext/OpenTorchlight`); its addresses are of a 64-bit
GCC build and do not apply here: every function below was found in GOG's executable (or the 360
image) by its shape, callers and callees, as stated for each.

### 9a. The 360's scheme

**The buttons** (the "How To Play" screen, `howto_xenon.uilayout`, its icons paired with their
labels by position): **[read]**

| Button | Action |
|--------|--------|
| left stick | move (the diagram does not label it); click (L3): automap |
| X | attack |
| A | interact, pick up an item |
| B, Y, LT, RT | skills, each assigned from the skill menu (`SkillSlotGamepadButtonX/Y/LT/RT` in the HUD) |
| LB, RB | health potion, mana potion |
| d-pad up/down | switch the active skill set (`SkillSet1`, `SkillSet2`) |
| right stick up/down | camera zoom |
| Back, Start | inventory and submenus; options |

**Reading the pad.** `sub_821FE1D0` polls XInput every frame and keeps each pad's state in a
16-byte record at 0x83557E80 (+8, +10 the left stick; +12, +14 the right one; +6, +7 the
triggers), with XInput's dead zones (7849 left, 8689 right); button edges become events
(`GamepadButtonDown/Up`, a CEGUI extension of Runic's: `GamepadEventArgs`). Getters:
`sub_82191018` (left stick, scaled by 1/32767), `sub_822DF6D8`/`sub_822DF738` (the triggers, 0..1),
`sub_822DF798` (a button or a stick direction, for menus). **[read]**

**Moving: the engine's click-to-move, with the stick's point instead of the cursor's.**
- `sub_8221C838(client, Vector3* out)`: if the left stick's length is over 0.1, the point is the
  hero's position plus the stick times 2.0 world units, rotated by the camera's yaw (from the
  camera's direction, `atan2`, minus 90 degrees). **[read]**
- `sub_8221C730` is the 360's `CGameClient::moveToMouse` *(OT)*: same shape as GOG's (below), with
  that point where PC has the world point under the cursor: if the point moved, it calls
  `sub_8228F8C8(hero, level, x, z)` = `CCharacter::setDestination(CLevel&, float, float)` *(OT)*
  and sets a 0.1 s timer (`client+0x40`), calling it again when the timer runs out. **[read; the
  names by shape: the same fields, the same timer, the A* path in the callee]**
- Every frame, the frame update (`sub_821EBA48`) also hands the point to `sub_8228F7E0(hero, x, z)`
  = `CCharacter::setTargetDirection(float, float)` *(OT)* (it stores the point and the normalised
  direction to it, as OT's does): the hero faces the stick. **[read]**
- So the 360 walks by pathfinding toward a goal 2 units ahead, renewed every 0.1 s, facing the
  stick every frame. It does not use `CCharacter::moveToPosition` *(OT)*: that one sets a position
  at once after `passableBetween` (OT's call graph: skill events use it), it does not walk.
  **[read: OT's call graph for moveToPosition; the 360's calls above]**
- The client's fields match between the 360 and GOG: +0x2c the hero, +0x38 the level, +0x40 the
  move timer. **[read]**

**Choosing a target (the 360's own code, per frame, `sub_821EB598`).** **[read]**
1. A cone search, `sub_821B58C0` = the engine's `CLevel::findCharacterWithinView(Matrix4,
   alignment, range, ...)` *(OT)* (a matrix of the hero, `atan2`, the same arguments), alignment 5,
   range 7.0 or the weapon's range plus 2.0 if longer; the result is the candidate at +0x1BC.
2. If none, the same search with alignment 3 and range 3.5, from a shifted origin.
3. An item search (`sub_821A4470`) for A: the item candidate at +0x1C4.
4. X attacks the candidate; moving the stick drops the current target (`sub_8221B8E8`,
   `sub_8221BC80`).
The cone's angle comes from a field of the unit (`CSkill::getFindTargetAngle` *(OT)* exists in
the engine); not read yet. **[read; the angle not yet]**

### 9b. PC's scheme (GOG build)

- **Polled mouse.** Small engine functions read the buttons with `GetAsyncKeyState` (`0x5cd4e0`
  the left button, honouring swapped buttons; `0x5cd510` the right; `0x5cd540` the middle) and the
  cursor (`0x5cd5a0`, `0x5cd550`). **[read]**
- **The click comes as a window message.** The executable subclasses its window
  (`SetWindowLongW`, procedure 0x403080 → 0x401730); mouse messages go to `0x40de60`, which calls
  `0x53ff90(msg, wparam)` (OT's `CGameClient::mouseEvent(unsigned, unsigned)` shape): it injects
  the button into CEGUI and queues the event (`0x4e7370`). **[read]**
- **Moving.** `0x4129d0` = `CGameClient::moveToMouse(bool)` *(OT)*: `findWorldLocation` *(OT)*
  (`0x411d60`, which reads the cursor itself with `GetCursorPos`), the hero's position
  (`0x510eb0`, `CPositionableObject::getPosition(bool)` *(OT)*), then `0x494b30` =
  `CCharacter::setDestination(CLevel&, float, float)` *(OT)* with `[client+0x2c]` (hero) and
  `[client+0x38]` (level), and the 0.1 s timer at `[client+0x40]`. `0x494b30` was identified by
  its callees matching OT's: three `CLevel::passableBetween` (`0x4f0340`, `ret 0x14`), three
  `CPath::AddPoint` (`0x5bf3d0`, `ret 0xc`), three `CPath::GetPoint` (`0x5bd950`, `ret 8`), two
  `positionPassable` (`0x4eeb40`), the game UI's `getUIIsInCinematic` (`0x5fe160` →
  `0x541db0`). **[read]**
- **The client.** The application object is `[0xc1ad64]`; its `CGameClient` is at `+0x64`
  (`CGameClient::update`, `0x415a00`, is called on it every frame). **[read]**
- **Attacking, skills, items** (OT's names, in the same engine; not located in GOG yet):
  `CGameClient::clickLeft` uses `mouseOverCharacter`, `CCharacter::setTarget(CCharacter*)`,
  `inAttackRange`, `attack`, `setTargetItem(CItem*)`; skills go through
  `CCharacter::updateSkillKeys`, `setActiveSkill(CSkill*, bool)`, `castSkill`; the cone search
  `CLevel::findCharacterWithinView` is in the engine (monsters use it). **[OT; to locate in GOG]**

### 9c. Prototypes (GOG, the virtual pad from a file; frames in `~/torchlight-gog/screens/`)

`proxy/pad.c`, `pad=1` in `x360_ui.ini`. The proxy points the executable's imports of
`GetCursorPos` and `GetAsyncKeyState` at its own (no code patched).

| Try | What | Result |
|-----|------|--------|
| a1 | the left button reads as held, the cursor 14 % of the height from the centre in the stick's direction | the hero turns, does not walk |
| a2 | plus the press and release injected into CEGUI | the same |
| a3 | the press and release as `WM_LBUTTONDOWN/UP` posted to the window (`pad_edges=message`) | **the hero walks** (`pad-prototype-a-messages.gif`) |
| b1 | `pad_mode=direct`: the proxy calls `setDestination`, client taken at `[app+0x3c]` (wrong) | nothing |
| b2 | the same with the client at `[app+0x64]` | **the hero walks** (`pad-prototype-b-setdestination.gif`) |

Option b, as run: while the stick is out of its dead zone, every 100 ms, the proxy asks the
game's `findWorldLocation` for the world point under the stick's screen point (the virtual cursor
holds only during that call), takes the direction from the hero to it (so it is relative to the
camera, as the 360's), and calls `setDestination` 2 world units that way: the 360's numbers. No
synthetic click, no held button: the mouse stays free. GOG addresses only (section 5). **[read:
the runs]** Option a3 works too, but leaves the game believing a button is held and a cursor sits
where it is not, which every hover and UI check sees; a3 is not the way. **[inferred]**

### 9d. Options for PC

| Option | What | Feels right | Feels wrong | Cost |
|--------|------|-------------|-------------|------|
| a. virtual mouse | stick → cursor ahead of the hero, synthetic clicks (a3) | works on any build without addresses | hover, tooltips and the UI see a fake cursor; attack and pick-up depend on what lies under the point; aiming is the mouse's | 2-3 days to make it usable; weak base |
| b. engine calls | stick → `setDestination` 2 units ahead every 0.1 s (done); X → `setTarget`/`attack` on a target of ours; A → `setTargetItem` | the 360's movement exactly (the same engine call, the same numbers); the mouse stays real | the targeting is ours to write (nearest, cone) | movement done; attack, pick-up, skills, potions 1-2 weeks |
| c. the 360's scheme on b | b plus the 360's own target choice: `findCharacterWithinView` cone, 7.0/3.5 ranges, item candidate for A, facing by `setTargetDirection` every frame | the closest to the 360 (its rules, the engine's search) | the cone's angle still to read; the 360's HUD prompts need a HUD | b + 1 week |

Pathfinding toward a point 2 units ahead is what the 360 itself does, so b and c inherit its
feel: the hero follows walls and turns around obstacles instead of sliding into them; the cost is
a short delay at a sharp turn (the goal moves every 0.1 s). **[read: the 360; the feel inferred]**

**Recommendation: c, on b's base.** Movement is validated. Next: locate in GOG `setTarget`,
`attack`, `setTargetItem`, `findCharacterWithinView`, `setTargetDirection` and the skill entry
points the way `setDestination` was located; then X attacks the cone's target, A picks up, B, Y,
LT, RT fire the skills in the hotbar slots, LB/RB drink potions, the d-pad switches the set.

**The gameplay HUD the 360 needs on PC:** the skill slots on buttons (`SkillSlotGamepadButtonX/Y/
LT/RT`, the 360's `hud.uilayout`), the two skill sets, the potion counters (`POTIONCOUNT`,
`MANACOUNT`), the target's name and health (`TargetName`, `TargetHealthBar`) and the interact
prompt (`TargetInteractHover`). PC's HUD has a hotbar of keys 1-0 instead. **[read: the layouts]**

**Menus with the pad (plan, not built).** CEGUI already draws a hovered state. When a layout loads,
the proxy lists its windows with an `onClick` (as the trace showed the game reading them); the pad
moves a virtual hover through that list (`MouseEnters`/`MouseLeaves` on the windows, what the
looks' `Hover` state follows in CEGUI 0.6), in reading order or by nearest neighbour in the
stick's direction; A fires the marked window's `onClick` (a click injected on it). Cost: 3-5
days for every `onClick` menu; screens with logic of their own (inventory, skills) are separate
(section 10). **[inferred]**

### 9e. Combat with the pad: first tries (2026-10-15, stopped after two hypotheses)

**The real pad works.** The user's session with the 8BitDo (XInput, `pad_mode=direct`): XInput
found, pad 0 connected, the stick and every button reached the proxy (log in
`~/torchlight-gog/x360_ui-user-pad.log`); the user: "se siente perfecto". **[read: the run]**

**Located in GOG** (names *(OT)*, by callees and shape; all `__thiscall`):
- `0x4949d0` `CCharacter::setTarget(CCharacter*)` and `0x494ab0` `setTargetItem(CItem*)`: the
  safe pointers at `+0x290`/`+0x298`, called from `clickLeft` (`0x4142a0`) with
  `[client+0x2c]`. **[read]**
- `0x5006c0` `CLevel::findCharacterWithinView(Matrix4 const&, alignment, float min, float half
  angle, float range, bool best, CCharacter* exclude, CSkill*)`: the only function with
  `Matrix4::inverse`, `atan2` (`fpatan`) and `ret 0x20`. `0x500a50` `findItemWithinView(Matrix4
  const&, half angle, range)`. **[read]**
- The hero's position is at `+0x70`; a 4x4 rotation at `+0xAC` (found in a dump). **[read]**

**The 360's target rule (read):** `findCharacterWithinView` with the hero's matrix, alignment 5
(units of alignment 2), min 0.5, a cone of **±30 degrees**, range 7.0 (or the weapon's + 2),
the best one; else alignment 3 within 3.5. Items: a cone of ±60 degrees within 3.0 (the 360's own
loop in `sub_821A4470`, same numbers). Constants at 0x820AA2C0 (+0x3B0 = 30, +0x240 = 60,
+0x1EC = 3, +0x400 = 7, +0x2F8 = 3.5). **[read]**

**Tries** (the mine, floor 1, from a restored save):
1. The cone oriented by the stick's last direction: X found no enemy; the engine's search with
   180 degrees found one 2 units away, behind the stick's direction.
2. The cone oriented by the hero's own matrix (`+0xAC`): X found no enemy either. **[read: the
   runs]**

What the log shows: the enemy candidate alternates every 100 ms between a unit and none, so a
press of X lands on "none" half the time (and the matrix at `+0xAC` gave the same facing as the
stick, so it may be the facing the hero is turning to, not the one it has). The next hypotheses,
not tried (the user's rule): keep a candidate until it is gone for 300 ms; find why the search
alternates (its `best` loop, or the unit's visibility flag); take the facing from the
orientation the engine renders (`getOrientation` *(OT)*). The monster the user saw die was killed
by the pet. **[read: the logs; the causes inferred]** GIF: `~/torchlight-gog/screens/combat-x-flicker.gif`.

**Diagnosis and fix (2026-10-15, the user's order: diagnose before more hypotheses).** A log line
per search, in the same frame: the stick, our `setDestination` calls and target drops since the
last search, the facing, the search's result, the hero's target (`+0x290`), state (`+0x284`) and
whether it is attacking. **[read: the runs]**
- **The alternation was not ours.** With the stick at rest our code calls neither
  `setDestination` nor `setTarget(0)` (the virtual pad rests at exactly 0, inside the dead zone;
  the 8BitDo's session logged its rest inside it too). With the cone oriented by the hero's own
  matrix the candidate is stable at rest; it changes only when the facing turns.
- **X did nothing because `setTarget` alone does nothing visible.** PC's click on an enemy
  (`clickLeft`, GOG `0x4142a0`, read against OT's): out of range, `stopPathing` (`0x486150`),
  `setTarget`, the hero's virtual `+0x174` with 4 (a mode), and its virtual `+0x1AC` with
  `(0.0f, level)`; in range, `stopPathing` and `CCharacter::attack()` (`0x491bc0`), after
  `performingAttackLoose` (`0x4863b0`), the state at `+0x284` not 3 and `performingSkill`
  (`0x486550`); `inAttackRange(0)` is `0x492160`. `setTarget` itself does nothing while the byte
  `+0x658` is set. X now does exactly that sequence, and repeats it every 100 ms while held (as a
  held mouse button repeats the click), unless an attack is under way.
- **Result: X attacks.** The hero chased the candidate (state 4), turned to it, attacked
  ("attacking" for 4 searches) and killed it (blood, "9 Gold"; the target cleared). The candidate
  is not held for 300 ms: the 360 does no such thing that we read, and it was not needed.
  GIF: `~/torchlight-gog/screens/combat-x-attacks.gif`. **[read: the run]**
- **Not working yet:** A (the item search `0x500a50` never returned an item, not even gold on the
  floor: its arguments or its identification are to be checked); LB (the hotbar key posted as
  `WM_KEYDOWN` did not reach the game in its one try, likely with the window unfocused); B, Y, LT,
  RT not tried. **[read: the runs]**

**X with no target, and the weight of the movement (2026-10-15, after the user's session).**
- **The 360 swings anyway.** With no candidate, its player controller (`sub_8221BC80`) takes the
  point ahead (the stick's, or the facing's at rest: `sub_8221C838` with r5 = 1), faces it with
  `setTargetDirection` and performs the button's action (`sub_82293548`). PC's equivalent is the
  hold-position attack: `setDestination` *(OT)* with the hero's hold byte set (`+0x20A` in GOG,
  the byte `moveToMouse` passes to `findWorldLocation`) stops, faces the point and calls
  `attack()` (read in OT's decompilation of `setDestination`). X with no candidate now does that,
  2 units ahead. **[read; not yet seen in a run]**
- **The 360's movement, as read:** `setTargetDirection` toward the stick's point every frame
  (`sub_821EBA48`), `setDestination` on a 0.1 s timer (`sub_8221C730`), and a last one when the
  stick returns to rest. GOG's `setTargetDirection` is `0x486180` (it stores the point at
  `+0x1CC` and the normalised direction at `+0x1D8`, as OT's and the 360's do; PC calls it in its
  hold-position input). **[read]**
- **Measured** (the mine save; the stick right, left, up, down, rest, twice; the time from the
  stick's change to the hero's facing within 20 degrees of the new direction; and how far the hero
  goes in the second after release): **[read: the runs]**

| Movement | Turn (ms) | After release (units) |
|---|---|---|
| before: `setDestination` every 0.1 s only | 117-312, mean about 206 | 1.42 to 1.76 |
| now: facing at once and on every call, `setDestination` every 0.1 s, a stop on release | 127-295, mean about 222 | 0.03 to 0.77 |

  The turn did not get faster: it is the engine's own turning speed, not our 0.1 s. What was
  heavy is likely the overshoot: on release the hero walked on to its last goal, up to 2 units;
  now it stops. (That `setDestination` at its own position stops it is inferred; the numbers
  agree.)
- **The stop, second version.** In the user's session (the real pad) the hero still walked on
  after release in most releases (up to 4.7 units, some of it maybe X's chases): a destination at
  the hero's own position leaves the end of the old path in GOG. The release now calls
  `stopPathing` (`0x486150`, what PC's `clickLeft` calls before an attack): 8 releases in 8
  directions, 0.00 units each. **[read: the runs]**
- **The user's session (real pad):** "anda mejor", still a little heavy; X: 25 swings in the air,
  6 chases, 5 attacks in range; A: 40 presses, `findItemWithinView` found nothing even at 180
  degrees and 20 units, so the list it walks or the object passed as the level is wrong, not the
  cone (left for later, the user's order); the hotbar buttons were not used (LB drank a potion in
  my run, without focus). **[read: the logs]**
- **Searches and the diagnosis log run only on a press** of X or A (and while X is held), no
  longer every 0.1 s.

**X like the 360, the candidate's highlight, the turn (2026-10-15).**
- **X never walks to its target.** The user felt "the game drives itself" when X chased. The
  360's X path (`sub_8221BC80`, the action `sub_82293548` and their callees) calls no
  `setDestination`: it faces the target and attacks where the hero stands. X now: in range,
  PC's attack; out of range, the hold-position attack toward the target; no target, the same
  toward the facing. The user: "parece casi idéntico". **[read; the user's session]**
- **The highlight.** The 360 marks its candidate. GOG's `CCharacter::setHighlighted(bool)` is the
  virtual at `+0x100` of the unit vtable (CPlayer's `0x488550`: it calls `CBaseUnit::
  setHighlighted`, `0x4812a0`, which stores `+0x170` and highlights the model, then highlights the
  12 equipped items; OT's code does the same; found by the getter after it, `+0x104`). The
  candidate is searched every 0.1 s again (the 360 does it every frame) and highlighted; the
  previous one is switched off only if it is still in the level's unit list (`[[level+0x5c]]`,
  nodes {unit, next}). It ran without trouble; the highlight itself was not seen yet (no enemy
  in the cone at the right moment). **[read; not yet seen]**
- **The turn is not the difference.** Both builds turn the hero toward its target direction with
  the same `turnToward` (the 360's `sub_821E0188`, GOG's `0x48d390`; the same constants: 180,
  360, a 5-degree dead band, ±1000), at the unit's `TURN_RATE` (the 360's `+0x218`, GOG's
  `+0x224`), loaded by the unit's init (`sub_822883F0` at 0x82288FB8; GOG at `0x4a04e0`) with the
  same default, 10.0, which the hero's data does not override in either pak. So the 360's turning
  in place when the stick spins fast comes from something else (when the hero starts walking while
  it turns); not found yet. **[read]**
- **The spin in place, again (2026-10-21; the user's "remolino": on the 360 a fast spin of the
  stick turns the hero in place, on PC it walks a spiral).** The 360's stick path read whole:
  `sub_8221B8E8` (the per-frame move: no destination during a skill, stops on release),
  `sub_8221C730` (a destination at once if the point equals the current one, else on the 0.1 s
  timer) and `sub_8221C838` (the point: the stick, 2.0 units, turned by the camera; past a 0.1
  dead zone) call `setDestination` (`sub_8228F8C8`, *(OT)*) as `proxy/pad.c` does. No test of the
  angle between the facing and the stick there. So the difference, if any, is in the engine's
  walking step (shared code whose two builds were not compared) or in timing. Not found; set
  aside, as the user allowed. The mods agent was asked whether it knows where the 360 decides
  to walk or only turn.

**The target choice, the 360's rule (2026-10-15, after the user: "lo tengo muy cerca y no me hace
el highlight; a veces me hace el highlight pero le pega a otro").**
- **X hits the highlighted candidate.** It searched again on the press and could pick another unit;
  now X uses the candidate already highlighted (it searches only if there is none).
- **The 360's rule** (`sub_821B58C0`, read in full): each unit that passes the engine's filters;
  within 0.5 at any angle, else within ±30 degrees of the hero's facing and the range (7, or the
  weapon's + 2), and with a clear line of sight beyond 0.5; the nearest (a priority by
  `sub_822A48D0`, not read, and a 5-degree angle tie). The matrix is the hero's world transform,
  its virtual `+0x6C`, which in GOG returns `+0xAC`: the one already used. The proxy now walks
  the level's unit list itself with GOG's own filters (those of `findCharacterWithinView`: alive,
  the virtual `+0x20`, the bytes `+0x6D` and `+0x161`, the alignment test `0x4eec80`, `0x40e6e0`)
  and `CLevel::sightUnobstructed` (`0x4fdd60`), with the 360's numbers, instead of PC's own
  "best" rule. **[read]**
- **In a run** (the mine, 7 units, 4 enemies after the filters): the nearest enemy was 2.96 units
  away at 63 degrees from the hero's facing, so not a candidate, as on the 360. If the 360 picks
  such an enemy, it is because its hero is already facing it (the turning, above), not because its
  cone is wider. **[read: the run; the cause inferred]**

**The three loose ends of combat (2026-10-15).** **[read; the runs]**
- **The turn rate is the same.** The hero's `TURN_RATE` in GOG at run time (`+0x224`) is 10.0, as
  the 360's default (`sub_822883F0`, 0x82288FD4). The 360's player controller writing `+0x218`
  (`sub_8219A7F8`) is not a turn rate: it fills a 4x4 matrix at `+0x1FC`. So nothing to set; the
  spin in place is still open.
- **The second pass of the target search.** The 360 (`sub_821EB598`): if no enemy is in the cone
  within 7, any unit (alignment 3) in the cone within 3.5 becomes the candidate. Done, with
  GOG's own filters; the 360 leaves out the hero's pet there (the first of its list at `+0x5AC`)
  and a unit `sub_82198E48` returns. GOG's version first left out every unit on the player's
  side instead (the alignment test with 1); that also hid the quest givers ("!") and the town's
  walking NPCs (the user: never highlighted, even when touching). It now leaves out only the pet:
  GOG's `0x40e720(index)` returns the hero's pets (its vector at `+0x5C4`, the 360's `+0x5AC`), as
  PC's `clickLeft` uses it (`0x4146c2`).
- **A, the 360's item rule** (`sub_821A4470`, read in full): the level's item list (GOG
  `[[level+0x68]]`), shown (`+0x19C`), the virtual `+0x20`, `+0x161`; first pass: within ±60
  degrees and 3, or within 0.5, gold counting 0.8 nearer, out of sight counting 10 farther; second
  pass: any within 2.0; the nearest. A then picks it up as PC's click on an item does. PC's
  `findItemWithinView` was the wrong function: it keeps only GOG's kind 0x1d, which the 360's rule
  leaves out. The unit kind numbers differ between the builds (GOG's 0x7a is not gold), and GOG's
  list also holds trigger units (A once "picked up" a `CTriggerUnit` under the hero): GOG's version
  keeps objects of class `CItem` and below (RTTI) and gives the bonus to `CItemGold`. In a run, A
  picked up a `CEquipment` next to two `CItemGold`. **[read; the run]**

**A picks up; items are highlighted; the searches' cost (2026-10-15).** **[read; the runs]**
- **The pick-up order was the breakables' one.** PC's click on an ordinary item (`clickLeft`,
  GOG `0x4146f8`; OT's branch for items not of its kind 0x1d) is: `stopPathing`, `setTargetItem`,
  the mode virtual `+0x174` with 7, `stopPathing`, and the virtual `+0x1B4` with `(0.0f, level)`.
  A used the attack branch's mode 4 and virtual `+0x1AC` (the user: "no pude agarrar nada"). With
  the item order, A walked to a `CEquipment` 1.5 away and picked it up (it left the item list).
  GIF: `~/torchlight-gog/screens/pad-A-pickup.gif`.
- **The 360 highlights the item A would pick up** (the user; its window with the item's name is
  left for later, from the 360's layouts). GOG's `CItem::setHighlighted` is the same slot `+0x100`
  (`0x4d11e0`: the text, then `CBaseUnit::setHighlighted`); the item candidate is searched every
  0.1 s and highlighted like the enemy.
- **The searches are cheap:** measured every 5 s on the game's thread, 1 to 50 microseconds on
  average, at most about 100. The performance drops the user felt are not them; not found yet
  (candidates: DXVK compiling shaders for new effects, the log written unbuffered). Gold is
  usually picked up by walking over it, as in PC.

**Movement while attacking; triggers; the performance drops (2026-10-15).** **[read; the runs]**
- **The 360 locks the stick's movement only during a skill.** Its move function
  (`sub_8221B8E8`) returns without a destination while `sub_8219A258(hero)` is true: a skill under
  way (`+0x2C4`), with its flag `+0x1FF` or its timer `+0x2B4` running and `+0x2B1` off (OT's
  `performingSkillLoose` shape). A plain attack does not lock it; the facing toward the stick
  goes on (it is set elsewhere, every frame). GOG's fields are the 360's + 0xC (`+0x2D0`,
  `+0x20B`, `+0x2C0`, `+0x2BD`); the proxy sends no destination while that holds. (The user
  asked whether the 360 blocks movement when attacking: only for skills.)
- **A picked up trigger units** in the user's session (7 times): GOG's `CTriggerUnit` derives
  from `CItem`; it is left out by class (the 360 leaves out its kind 0x1d there).
- **The performance drops were the log.** In the user's session a search took up to 52 ms; the
  searches themselves take at most tens of microseconds (measured apart: units and items); the
  rest was the diagnosis lines written, through Wine, during the 3 s after each press of X or A.
  That log is now off unless `pad_log=1` is in `x360_ui.ini`.

**The movement measured (2026-10-24).** Two test options of `x360_ui.ini`, named on the screen's
test notice like the others: `pad_replay=PATH` plays a recorded stick (`tools/pad_trace.py
routes`) in place of XInput, from 3 s after the first frame in play (the file's `# start_ms`); with
the option set the hand's pad works until the first frame in play (the title, the main menu's
Continue) and is not read after it (the menus too get the replay's buttons then, the last line's
after the end), and the replay's clock is the performance counter (Windows'
`GetTickCount` moves by 15.6 ms). `pad_trace=1` writes the hero's path to `x360_trace-<date>.csv` in the
mod's folder: the time, its position, its facing, GOG's `+0x200` and `+0x204` and the raw stick,
a line whenever one of them changes, until 2 s after the replay's end, with a last line there. If
the pad's input is held by a menu, a tip or a cinematic while the replay runs, the log says
"replay disturbed" and that run does not count. The formats are in
`proxy/pad.c` and `tools/pad_trace.py`; the recompiled 360 build writes the same trace from its
own dev input script, so `pad_trace.py compare` can set the two side by side. The tolerances
proposed: the two paths at most 0.5 units apart at the same time (about the hero's width), to the
later trace's end (the slide after the release counts); and in each 250 ms span where the 360's
hero moves at a steady speed (not a start or a stop), the speed within 10% of the 360's, allowing
one 30 fps frame of lag; none moving where it rests.

**The owner's route recorded (2026-10-24).** The routes are the owner's own, played on the pad
and recorded, not written by a tool: `pad_record=1` (a test option, off by default; it turns
`pad_trace` on too) writes `x360_record-<date>.txt` in the mod's folder, a replay file
(`# start_ms 0`, t=0 the first frame in play, a line when the stick, a trigger or a button
changed, the buttons as the game reads them), and the trace of that session beside it. That trace
is the baseline. `pad_replay=` plays the recording back by time, so it runs the same on this build
and on the recompiled 360's whatever their frame rates; each change to the walk is measured by
playing the same recording and comparing the traces (`pad_trace.py compare`). A replay file is
loaded on the process heap (64 MB at most), so a long recording fits. The save: a copy of a hero
saved in town, through `save_root=`, so the session starts at the town's start point, as the
recompiled build's does from a town save. The recording is written to its end when the game
exits (`DLL_PROCESS_DETACH`), and when the pad drops out its line at rest is written too; a replay
is cut at 70 minutes (the trace's microseconds in 32 bits). The tool's own routes
(`pad_trace.py routes`) start to the left (`--angle`, 180 by default), the open ground at the
town's start point.

**Doors and barrels (2026-10-15, two regressions the user found).** **[read; the logs]**
- GOG keeps doors and levers as `CTriggerUnit` and barrels as `CBreakable` in the level's item
  list; both derive from `CItem`. Leaving the trigger units out of A (above) stopped A from
  opening doors: they are back in, and A works them with PC's item order (mode 7, `+0x1B4`), as a
  click does. Barrels are what the 360 leaves out of A (its kind 0x1d; PC's `findItemWithinView`
  keeps only that kind, and PC's click attacks it): A had walked to a barrel and broken it, which
  the 360 does not do; they are out of A now.
- **Open: X on barrels.** On the 360, X attacks a barrel in front of the hero. Its unit search does
  not walk the item list and its item search leaves barrels out; the functions that test the kind
  0x1d (`sub_821A72A0`, `sub_821A83C8`, `sub_821A6668`, ...) are no cone searches. Not found yet.

**The user's session with doors, barrels, NPCs, portals (2026-10-15).** **[the user; the log]**
- Dungeon chests and the town portal: work with A. Barrels: no highlight, not broken. NPCs:
  highlighted, but A did nothing. The shared stash (the town's chest): A did not open it.
- **NPCs are `CMonster`** like the enemies; A only set the target. The 360 does not walk to them:
  A sets the target and the mode 9 (PC's click on a non-enemy, GOG `0x41461c`, without its
  `setDestination`): the engine's `approachAI` *(OT)* opens the dialog by the NPC's kind if it is
  in reach, or drops the target. The user: on the 360 the hero talks from where it stands.
- **Barrels**, by the user's account of the 360 (highlighted; X breaks them; A does not touch
  them): they are back in the item candidate (so highlighted); A skips a `CBreakable`; X, with no
  enemy in the cone, breaks the item candidate if it is one: in range, `setTargetItem` and
  `attack()` (PC's click on a kind 0x1d item); out of range, the attack in place toward it (X
  never walks). Where the 360's own code does this is still not found (its item search reads as
  leaving kind 0x1d out; so 0x1d may not be the barrels' kind).
- **The shared stash:** class unknown yet; with `pad_log=1` the near list on each press shows it
  if it is in the unit or item list.
- The user asked to be told before every launch and to launch only with their OK.

**Responsiveness, X walking, the highlights' distance (2026-10-15, after the user: "el de 360 se
sigue sintiendo más responsive"; X sometimes walks before attacking; barrels not highlighted; the
360 highlights NPCs almost touching, ours from far).** **[read; the user's run]**
- **The settings:** GOG runs at 1920x1080, `MAX_RENDER_FPS 60` (the game's own cap, not the
  driver's), `VSYNCH 0`, about 51 fps in `local_settings.txt`. The proxy read the pad only every
  50 ms and chose the candidate every 0.1 s; the 360 does both every frame. Now 4 ms and 15 ms.
- **X walked:** out of range, X set the enemy as the hero's target and then called
  `setDestination` with the hold byte. Its hold branch (GOG `0x49511b`) puts the destination at
  the hero, calls `attack()` and aims at the point; but `attack()` with a target set goes to it,
  and `setDestination` drops the hold during a skill's chain (`+0x2d0`, its bytes `+0x3c`, `+0x40`,
  `+0x20b`, `+0x2c0`) and walks to the enemy's position. The in-place attack now clears the
  target and the item target, `stopPathing`, `setTargetDirection` toward the point, and `attack()`
  with the hold byte set (it needs a target, an item target or the hold byte: `+0x290`, `+0x298`,
  `+0x20a`).
- **The highlight's distance:** the 360's second pass (`sub_821EB598`, 0x821EB8B8) copies the
  hero's matrix and moves its origin by -1.5 times the hero's facing (virtual `+0x94`; in GOG the
  same slot returns `+0xa0`, the third axis of the hero's orientation, the facing the cone uses),
  then searches ±30 degrees within 3.5 from there: any unit is a candidate only within about 2
  ahead of the hero (0.87 to each side at the hero). The proxy searched from the hero, so NPCs
  lit up 3.5 away. Now from 1.5 behind. (Its first pass, enemies within 7, is from the hero; the
  range becomes the weapon's +2 when larger, `+0x3ec`, not ported.)
- **The 360 keeps its candidates in the client at `+0x1bc` (unit) and `+0x1c4` (item)**, and GOG's
  client still has those fields: its hover code (`0x41b59d`) highlights them when the mouse is over
  nothing, and its click (`0x4148d5`) takes them. The proxy still calls `setHighlighted` itself;
  writing those fields (watched pointers, `0x5ff800`) is an option for later.
- **Barrels:** the highlight did show (the log: `+0x170` = 1 on all 7, none switched off); the
  user then said the 360 does not highlight barrels. They stay the item candidate, so X still
  breaks them, but the proxy no longer highlights them.
- **The user's run:** "salió todo ok" (X: 82 attacks in range, 24 in place out of range, 29
  swings). Steam UNCHANGED. **[the user; the log]**

**A save to test from:** `~/torchlight-gog/save-backups/mine-level1/` (the GOG prefix's `save/`
at the entrance of the mines' first floor, with its settings: the same map every time). Restore it
with the game closed into the prefix's `runic games/torchlight/save/`.

## 10. The 360's screens with logic of their own, against PC's (2026-10-15)

Sources as in section 9. The 360's classes and their bases are read from the image's RTTI (the
class hierarchy descriptors); PC's names are OpenTorchlight's *(OT)*, the same engine.

### 10a. The same model under both

- **The model classes are on both:** the 360 image has RTTI for `CInventory`, `CEquipment`,
  `CEquipmentRef`, `CItem`, `CItemGold`, `CGameUI`, `CSubMenu`, `CPlayer`, `CCharacter` and the
  interface `iInventoryListener`, the classes OT names in PC's item code. **[read]**
- **The 360's screens are new views over that model:** `CInventoryMenuXenon`,
  `CStashMenuXenon`, `CMerchantMenuXenon`, `CEnchantMenuXenon`, `CSkillMenuXenon`,
  `CStatsMenuXenon` and `CQuestMenuXenon` all derive from `CMasterHousingXenon` (the 360's tabbed
  frame, switched with LT/RT, `mastermenu_xenon`) and from `CSubMenu`, PC's menu base too;
  `CInventoryMenuXenon`, `CInventoryXenon` (its list) and `CCharacterMenuXenon` also implement
  `iInventoryListener`, as PC's `CInventoryMenu` does *(OT)*: they are told of the same model
  events (equipped, unequipped, picked up, dropped, used). **[read]**
- **PC's entry point for every item screen:** `CGameUI::menuItemClick(CCharacter*, CSubMenu*,
  int slot, bool)` *(OT)*, the handler of a click on a slot in the inventory, stash, merchant,
  pet and enchanter screens. By OT's call graph it does the whole job: take and drop
  (`CInventory::pickupEquipment`, `removeEquipment`, the dragged item kept by `CGameUI`,
  `returnDraggedItem`), equip (`canEquip`, `equipEquipmentIntoFirstFreeLocation`,
  `findEquipmentSlot`), compare (`getComparisonItems`), sell and buy (`sellPrice`, `buyPrice`,
  `giveGold`, `CPlayer::soldItem`), drop to the ground (`CLevel::addItem`), the shared stash, the
  sounds. `CInventory` *(OT)* is the model: panes and slots (`getPaneSize`, `getItemPane`,
  `getEquipmentInSlot`, `findFreeSlot`), equipping (`equipEquipmentIntoSpecificLocation`,
  `unequipEquipment`, `swapWeaponSet`), `useEquipment`. **[OT; to locate in GOG]**
- **So the approach the user proposed holds:** a controller of ours in the proxy that drives the
  360's layout and calls PC's model, mostly through the same `menuItemClick` PC's mouse uses
  (one call per pad action on a slot index), plus `CInventory`'s getters to fill the 360's
  rows. Whether the 360's own code calls `menuItemClick` or `CInventory` directly is not read
  yet; it does not change the approach. **[inferred]**

### 10b. The screens

| 360 screen (class; layout) | What it is on the 360 | PC's counterpart *(OT)* | Cost on PC |
|---|---|---|---|
| Inventory (`CInventoryMenuXenon` + `CInventoryXenon`; `inventorymenu_xenon`) | **a list, not a grid**: 5 visible rows (`ItemTray0-4`: name, quality, damage/armour, enchantments, sockets, gold, an "equipped" mark), filters by kind (weapons, armour, jewellery, potions, spells, fish) on LB/RB, scroll arrows, a buy-and-sell mode (`buyandsell`) and the transmuter | `CInventoryMenu` (a grid of 63 slots in tabs and a paperdoll; drag and drop through `CGameUI`) | 2-3 weeks, with the shared infrastructure |
| Character (`CCharacterMenuXenon`; `charactermenu_xenon`, `statsmenu_xenon`) | stats and the points to spend (`strPts`, `dexPts`, `magicPts`, `defPts`, `spendPoints`, `removePoint`) | the stats menu (`CStatsMenu`) | 1 week |
| Skills (`CSkillMenuXenon`, `CCharSkillsMenuXenon`; `skillsmenu_xenon`) | 3 trees of 12 skills (36 cells), 4 spells, **8 pad slots** (`SkillSlot0-7`: 2 sets of X, Y, LT, RT... as the HUD) | the skill menu (`setActiveSkill`, `spendSkillPoint`) | 2 weeks (needs the pad skill bar of section 9) |
| Pet (`petmenu_xenon`, `petcommands_xenon`) | stats, 2 spells, commands (to town, feed) | `CPetMenu` | 1-2 weeks |
| Stash, merchant (`CStashMenuXenon`, `CMerchantMenuXenon`) | the inventory's list with a second list (buy and sell, transfer) | `CStashMenu`, `CMerchantMenu`, `menuItemClick` | 1-2 weeks each, on the inventory |
| Enchanter (`CEnchantMenuXenon`; `enchantermenu_xenon`) | an item, a price, one button per action (enchant, add or destroy a socket, retire) | `CEnchantMenu` | 1 week |
| Quests, journal, stats (`CQuestMenuXenon`, `CJournalXenon`, `CStatsXenon`) | lists to read; quest rewards in rows | `CQuestMenu`, journal | 1 week each |
| The tabbed frame (`CMasterHousingXenon`; `mastermenu_xenon`) | LT/RT between the screens, item compare toggle, pet commands | none (PC opens each screen by its key) | 1 week |

Shared infrastructure, once (1-2 weeks): the proxy hosts a 360 layout as a window of its own
(loaded with the real `loadWindowLayout`, attached to the root, shown and hidden), the controller
framework (pad events, focus, the rows), and locating PC's functions in GOG the way
`setDestination` was located (then again in Steam's build). **[inferred: the costs]**

**The 360 inventory's pad handling** (`CInventoryMenuXenon`'s `GamepadButtonDown` handler,
0x8236FEF0, subscribed with `MouseMove` in 0x8236F3E0; Runic's button codes from `sub_822DF308`:
d-pad 1-4, A 5, B 6, X 7, Y 8, RT 9, RB 10, LT 11, LB 12, Start 13, Back 14, the left stick 17-20,
the right one 21-24): **[read]**
- d-pad left and right, A, X, Y: one action function (0x8236A810), which works on the selected
  row of the list (`CInventoryXenon`, at the menu's +0xC0);
- B and Back: close; RB and LB: one function (the filters, by the layout's bumpers); RT, LT: the
  housing's tabs;
- the left stick up and down: the row selection; left and right: two other functions.
- **Re-read 2026-10-22 (the jump table at `0x8236FFE4`, index = code - 3):** RB (10) and LB (12)
  go to `0x823702BC`, which does nothing; the filter is changed by codes 19 and 20 (the left
  stick's left and right by the table above): 19 the list's `+296` minus 1 (wrapping, the sound
  67; past the first, `sub_823702E0`, the column before), 20 `sub_823653C8(list, 1)` (past the
  last, the next column); 17 and 18 move the row (`sub_8236A488`). This contradicts the line
  above and the port (LB/RB change the filter; the layout shows the RB icon by the filter's
  name). Runic's codes come from XInput's keys (`sub_822DF308`, its table at `0x822DF334`):
  `VK_PAD_RSHOULDER` 10, `LSHOULDER` 12, `LTHUMB_LEFT` 19, `LTHUMB_RIGHT` 20, `RTHUMB_LEFT` 23,
  `RTHUMB_RIGHT` 24. RB and LB go to `0x823702BC`: `sub_823702E0(menu, 0)`, the other column with
  its own filter (the mods agent saw RB switch My Bag and Pet Bag on the 360). **The port now
  does this**: LB/RB the column, the left stick's left and right the filter (past the ends the
  column, as before), at the merchant's menu too (`sub_8237A0A8` sends 19 and 20 to the filter
  the same way; its RB and LB cases were not aligned exactly). Not run yet. **Confirmed on the
  360 (the mods agent, 2026-10-22, with the stick's keystrokes): one push right is one filter
  step (Weapons, Armor, Jewelry, Miscellaneous, Socketables); RB/LB switch My Bag and Pet Bag.**
  Earlier: the
  mods agent's scripted left stick (four pushes right, two builds) did not change the 360's
  filter either; but its script driver sends no `VK_PAD_LTHUMB_*` keystrokes (the mods agent read
  its `PadKeystrokes`: buttons, d-pad and triggers only), so that test says nothing of codes
  19/20. The reading stands; it waits for a real pad.
  Two readings of the filter's input have now been contradicted or left open: stopped here until
  a run with a real pad (the owner's guided capture, or run B2).
What A, X and Y do to the item (equip, drop, compare: the layout's `ToggleItemCompare` and the
help texts) is not read yet: it is the first task of the slice. **[read: the dispatch; the
actions not yet]**

### 10c. The vertical slice: proposal (for the user's approval)

**Screen: the inventory**, as the user suggested, with one correction to the scope: the 360's
inventory has no cells and no drag; it is a filtered list where A acts on the selected row. So
"move a cursor over cells, take and drop an item" is PC's model, not the 360's. The slice:
1. Read the 360's actions (A, X, Y, and what the list shows per row) in 0x8236A810 and its
   callees; locate in GOG `CGameUI::menuItemClick` and `CInventory`'s getters (as
   `setDestination` was).
2. The proxy opens the 360's `inventorymenu_xenon` on the pad's Back (PC's inventory stays
   closed), fills its 5 rows from the hero's `CInventory` (name, quality, the equipped mark), and
   handles: the stick up and down (the selection and the scroll), LB/RB (the filters), A (equip
   or use the item: `menuItemClick` on its slot, as PC's right click does), B (close).
3. Checked in the game: the list matches PC's inventory, A equips a weapon and PC's paperdoll and
   stats show it, the game's save keeps it.

Why the inventory and not another: it is the most used and the one that exercises the most of the
stack (a list filled from the model, an action through PC's own handler, the model's events back).
The enchanter would be smaller (one item, a few buttons) but tests less; the skills need the pad's
skill bar first. Estimated 1-2 weeks for the slice, of the 2-3 for the whole inventory.
**[inferred]**

## 11. The inventory slice: the 360's inventory in PC (2026-10-16)

Branch `inventory-x360` (from `main` after `gameplay-pad` was merged). Code: `proxy/x360_menu.c`;
the pad's Back opens it (`proxy/pad.c`). Screenshots: `~/torchlight-gog/screens/x360-inventory-*.png`.

**Hosting.** The converted `inventorymenu_xenon.layout` (stage 0, `media/ui/x360/`, from the
user's pak) is loaded once with the real `WindowManager::loadWindowLayout` (prefix `x360inv_`),
attached to PC's GUI sheet (`System::getGUISheet`, `Window::addChildWindow`) and shown or hidden
(`setVisible`, `moveToFront`); PC's inventory is not opened. Its windows are found with
`WindowManager::getWindow` by their prefixed names. It needs `mode=scheme` (the 360 scheme). **[the runs]**
- **Assets:** it uses only the imagesets `widgets`, `widgets1`, `widgets2` and the fonts `Serif`,
  `Serif14`, `SerifBig`, `SerifHuge`, `SerifSmall`, all taken by the converter; not `widgets3`-`6`
  or `characters`. **[read]**
- **Scale: one factor, by height, height / 768.** The 360's fonts are PC's (the same TTFs and
  sizes, `NativeHorzRes 1024`, `NativeVertRes 768`, `AutoScaled`), so they grow by height / 768;
  the layouts' pixel offsets are scaled by the same factor on load (CEGUI 0.6 does not scale
  them). The 360's screenshots agree: its 919x690 frame is 1290x967 at 1080 (1.40). (Height / 720,
  the first try, made the frames 7% larger than the fonts.)
- **Position: the 360's frame.** `mastermenu_xenon` (919x690, centred; LT/RT tabs, the title, the
  bottom bar) is loaded too; its `ContainerFrame` (902x630, centred, +19) holds the screens in
  three columns of 301 (the character, the bag, the pet's bag); the bag's `TopFrame` is moved into
  it at x = 301. Side by side with the 360's screenshots the frame and the bag fall on the same
  pixels ((315, 55) and (747, 127) at 1080): `~/torchlight-gog/screens/side-*.png`. The 360's tabbed frame (`mastermenu_xenon`) places the panel;
  here it is moved to where the 360 shows the bag, the frame's middle column ((752, 128) at
  1080 in the 360's screenshots, `~/torchlight-gog/screens/ref/`). **[the runs; the screenshots]**

**What the 360 does (read in the image):**
- **Buttons** (`sub_8236FEF0`, `sub_8236A810`): d-pad left/right toggle item compare (below); A acts
  (`sub_8236AA48`); X sells (`sub_8236AD18`, in a shop); Y drops (`sub_82369E80`) or transmutes;
  the left stick's left and right the filter (0 to 5, past the ends the other column; RB/LB the
  column: corrected 2026-10-22, below); B closes.
- **The list** (`sub_82369FE8`): the owner's inventory references, the equipped ones first, then
  the others, both from the last to the first; an item is listed when its category equals the
  filter or is 6. **The category** (`sub_82369C48`, unit types of `UNITTYPES.HIE`, the same 162
  IDs in the 360's pak and PC's): POTION, SPELL, SCROLL, MAP, QUESTITEM → 3; WEAPON → 0; ARMOR → 1;
  RING, NECKLACE → 2; SOCKETABLE → 4; FISH → 5; anything else 6 (every filter). The fifth tab's
  image is `tab_gems`. **[read]**
- **Five rows** (`ItemTray0-4`), the scroll and the selection kept per filter (menu `+0xF4`,
  `+0xDC`). Each row's windows (`sub_823660E0`): tray, `Slot`, `SlotText`, `Requirment`, `Quality`,
  `DamageArmor`, `Enchantments`, `Gold`, `QualityBk`, `Equipped`, `qualityarrow`, the sockets.
  `sub_82368980` fills a row: all its children shown, then `Requirment` and `Gold` hidden when
  their text is empty, `Enchantments` shown only for an identified, enchanted, non-consumable
  item, `Equipped` by whether it is equipped; a row with no item is hidden. **[read]**
- **The selection** (`sub_823652A0`): `moveframe` (the frame with the button bar under it) is
  put at the selected tray's y minus 18 (scaled); the tray's image is `item_tray`, or
  `item_tray_equipped` for an equipped item; the selected row's alpha is 1.0, the others' 0.9 if
  equipped, else 0.6. **[read]**
- **The header** (`sub_82367D38`): `playergoldtext` is the owner's gold; `inventorysize` is
  "references/capacity" (4 in the transmuter, else 50 or 126 by the owner; the 360's screenshots
  show 48/50 for the hero and 0/50 for the pet). The
  layout's "12423" and "3/25" are samples. `IDENTIFYWIN` ("Scroll x12") is hidden on every rebuild.
  **[read]**

**PC's side (GOG, located and checked in runs):**
- `CInventory` is the character's `+0x404`; its references at `+0x1C` (count `+0x20`); a
  reference holds the item at `+0x8` and its slot at `+0xC`; slots 0-11 are equipped
  (OT `isEquipmentEquipped`), the bag's panes start at 19, 40, 61 (`+0x60`), 82 slots (`+0x18`).
  The item's name is the `std::wstring` at `+0x1B0` (found by a probe of the item's strings:
  "second blade"); its icon window `+0x24C` (`createIcon`, `0x4b9b70`), whose `Image` is PC's own
  atlas image (`set:itemicons8 image:hacker`, ...): the icons come from PC's atlases by PC's data,
  so none is empty. Gold: `+0x3C4` of the last owner (`+0x5B4`; `giveGold`, `0x487f20`).
- **A** does what PC's right click on an inventory slot does (`CGameUI::processIngameInput`
  *(OT)*, GOG `0x55e156`): `isUseable` (`0x53ea10`) → `CGameUI::useItem` (`0x5501b0`; `CGameUI`
  is the client's `+0x3C`, its hero at `+0x1C`); else `equipEquipmentIntoFirstFreeLocation`
  (`0x4e9fe0`), else the swap (`getComparisonItems` `0x4e7b90`, `removeEquipment` `0x4e9ca0`,
  equip, `pickupEquipment` `0x4e9a30` of the old one), with `setRenderBehind` and the drop sound.
  An equipped item is taken off: `removeEquipment`, `pickupEquipment`, back on if the bag is full.
- In a run: A took the second blade off (the hero empty-handed) and put it back; the potions'
  filter listed the four consumables with PC's icons and names; "4/63" and the hero's 100 gold.

**The 360's screenshots** (`~/torchlight-gog/screens/ref/`, made by the recomp's mods session
from replayed captures): the bag is the middle column of a three-column frame (the character and
the doll on the left, the pet's bag and the selected item's Info card on the right, Quest and
Stats tabs on LT/RT); filter 3 is "Miscellaneous"; a row shows the name, "DPS: n" or the stack
"xN", the price with a coin, "Equipped"; an unidentified item's name in red with "???" and "Press A
to identify"; the action bar outside a shop is A Take off / Use / Learn, Y Drop, X Swap.

**Labels and the action bar** (the 360's English strings, its image: `My Bag`, `Pet Bag`, the
filters `Weapons`, `Jewelry`, `Miscellaneous`, `Socketables`, `Fish` (Armor is not in that
table), the actions `Use`, `Equip`, `Unequip`, `Drop`, `Swap`, `Buy`, `Sell`, `DPS:`): the bar
in the screenshots is A by the item (Unequip on an equipped one, Equip, Use, Learn on a spell),
Y Drop, X Swap, for every item outside a shop. Y drops the item at the hero's feet (PC's drop
of an item that does not fit: `removeEquipment`, `CLevel::addItem` `0x4f5960`, the item's
virtual `+0x198`); X moves it to the pet's bag (the pet's `CInventory`, its `+0x404`; "Swap" as
the pet's column is **[inferred]**). "Learn" is the Spanish "Aprender" in English **[inferred]**.
Not working yet, so hidden: the LT/RT tabs, "pet commands", "item comparison".
- **Open after the side by side:** "Miscellaneous" is clipped in the filter's box (the
  screenshots are Spanish: an English capture of the 360 would show how it fits); PC's bottom HUD
  shows under the frame's bottom edge.

**The rows' details** (as the screenshots show them; the 360's strings `DPS:`, `???`, `Press A
to identify`): the price with its coin from PC's `CEquipment::sellPrice` (GOG `0x4b3a90`); "DPS:
n" for an identified weapon (`CEquipment::DPS`, `0x4b8f10`, a float); "xN" for a stack (`+0x1E4`);
an unidentified item (`+0x2FC` zero, OT's `+0x348`) with its name in red (`FFFF2A2A`, the
screenshots' red without the row's 0.6 alpha), "???" and "Press A to identify". The identified
flag's sense is read from OT and is checked in the next run's log. **[read; not run yet]**
- Still missing: armour values, the enchantments' line, the quality colour, sockets, the
  comparison arrow; A on an unidentified item identifies it on the 360 (here it equips).

**The letters' width.** On identical letters ("My Bag" against "Mi Bolsa") PC's text is 6-10%
wider at the same height. The recomp's mods session: at render scale 1.5 the 360's CEGUI still
lays out and rasterises its fonts at 720p and the font textures are sampled up 1.5x; PC rasterises
at 1080 with its own rounding and hinting. Both stretch the glyphs the same way in 16:9 (CEGUI's
AutoScaled fonts: width / 1024 across, height / 768 up). English captures of the 360 would settle it.

**Matching the 360's letter spacing, sharp** (the user's order, 2026-10-16). The converter now
renames the 360's fonts `X360_<name>` (font files, scheme, layouts and looknfeel `Font`
properties), so they are fonts of their own and PC's `Serif`, `SerifBig`, ... are untouched. The
proxy, before loading the layouts, lays each `X360_` font out once at 1280x720 with CEGUI's own
FreeType (`Font::notifyScreenResolution`, exported, virtual; CEGUI 0.6.2's `FreeTypeFont::
updateFont` sizes a font by `NativeHorzRes`/`NativeVertRes` and keeps `horiAdvance` of each glyph,
autohinted), keeps every advance (U+0020-U+024F; `Font::getGlyphData`, a `FontGlyph` {image,
advance}), lays it out again at the game's size (the glyphs rasterised there, sharp) and replaces
each advance by the 360's times height / 720. It checks the advance of "e" on every opening and
does it again if CEGUI laid the font out anew. **[read: CEGUI 0.6.2's source]**
- **Runs:** laid back out at the window's size (1920x1080) the fonts came out larger than before
  ("Unequip" ran into "Drop"): the game's fonts are not laid out for the window's size. Unpatched,
  the renamed `X360_` fonts are also larger than PC's `Serif` from the same file (a font the
  scheme creates gets another size than the game's), and PC's were the ones that matched the
  360's screenshots. So the fonts are laid back out for PC's `Serif`'s own size, read from its
  object (its scalings before `NativeHorzRes` 1024 and `NativeVertRes` 768, within its first 1200
  bytes). **[the runs; the last change not run yet]**
- **What it was:** the game lays its own fonts out with a native width of 1365, not the files'
  1024 (PC's `Serif` in a run: scalings 1.406 and 1.406, native 1365x768), so PC's text is not
  stretched across; the renamed `X360_` fonts kept the files' 1024 and came out 33% wider. Now
  each `X360_` font gets the game's native size (1365x768, copied from PC's `Serif`), is laid out
  at 1280x720 (scaling 0.9375 both ways, the 360's) to read the advances, then for PC's size
  (1920x1080), and its advances are set to the 360's times height / 720. Side by side with the
  360's English screenshots (`~/torchlight-gog/screens/side-en-*.png`): the town portal scroll's,
  the mana and health potions' names take the 360's width and the misc filter's label fits its box. **[the run]**
- **A crash on the way:** CEGUI's `setProperty` faulted after the icon background's "can equip"
  check: CEquipment's virtual `+0x164` (GOG `0x4b8660`) takes two arguments (`ret 8`: the
  character and a flag; the 360 passes the hero and 1), and one was passed. The same call in A's
  swap was fixed with it.
- **The used potion** now stays in its row with its stack lowered (a run: health potion x2 → x1,
  its price 60 → 30). The weapons' DPS is an int (`fistp` into `eax`), not a float.

**A potion used from the list vanished until the next opening** (the user's run): PC's `useItem`
takes the stack out while the hero drinks and puts the rest back, and the list was rebuilt only
on our buttons. The screen now rebuilds whenever the inventory's fingerprint (items, slots,
stacks) changes, checked every 0.1 s while it is open, as the 360's screen hears every change
through `iInventoryListener`.

**The frame's three columns** (2026-10-16, night). **[the runs: `x360-gog-frame-3columns.png`,
`x360-gog-swap-to-pet.png`]** The pet's bag, the focus moving to it with RB and X moving an
identify scroll to it (3/63 and 1/63) were seen in runs.
- **The pet's bag**: the same layout loaded again (prefix `x360pet_`) in the third column (x = 602),
  filled from the pet's `CInventory` (the hero's pet by `0x40e720(0)`, which returns 0 without a
  pet; then the layout's "Your pet has gone to town."). The 360 (`sub_823702E0`, its bags at the
  housing's `+0x104` and `+0x108`, the focus flag `+0x124`): LB/RB past the first or the last
  filter moves the focus to the other bag, at its last or first filter. The unfocused bag shows no
  selection frame and its rows dimmed. X moves the item to the other bag, both ways. A in the
  pet's bag does nothing yet.
- **The character column**: `charactermenu_xenon` (prefix `x360chr_`) in the first column. PC's
  own stats menu (`STATSMENU.LAYOUT`) has windows of the same names (`CharacterName`,
  `CharacterTitle`, `Level`, `XP`, `FameLevel`, `Fame`, `HP`, `Mana`, `Strength`, `Dexterity`,
  `Magic`, `Defense`); the proxy keeps its root when the game loads it and copies their texts.
  The 3D doll: below (2026-10-16, the doll, the Info card, PC's HUD).
  - **The numbers, read through PC's own functions (the user's order, 2026-10-16):** CStatsMenu's
    update (GOG `0x5b0d90`) reads them so: the getters OT names `CCharacter::strength`,
    `dexterity`, `magic`, `defense` (GOG `0x487ec0`, `0x487e60`, `0x487d70`, `0x487d00`: the base
    `+0x3AC`.. plus effects), the maximum HP `0x4901e0` and mana `0x490330`; no getters for the
    level (`+0xF0`), the experience (`+0x3C8`), the fame (`+0x3CC`) and its level (`+0x3D0`): PC's
    menu reads those fields itself, and so does the proxy; the next level's experience and fame
    from the experience table (`0x5281d0`, then `0x528130` by level, `0x528180` by fame level);
    the name a wstring at `+0x420`. A run read them right (level 1, XP 130/4000, fame 0/1600, HP
    300, "<the character's name>"). The fame title: below.
  - **They did not show at first (stopped after two hypotheses; solved below).** The windows are found, visible,
    scaled, and hold the texts (read back: "300", "1", "<the character's name>"), yet nothing is drawn; the
    layout's own "MP" label is not drawn either, while "HP", "Level" and "Stats" are. Neither
    taking `AlwaysOnTop` off nor setting `TextColour` white changed it. Open.
  - **Stopped after two hypotheses (2026-10-16).** PC moves its stats menu's windows away from the
    root it loaded (`11_Master`, no children after start); they are found by their full names
    (the root's prefix plus the name), but PC writes their texts only in its `CStatsMenu`'s update
    (vtable `0xab87c4`, the virtual `+0x24`, `CSubMenu::update(float)`, GOG `0x5b0d90`, 3000
    instructions: the level from the hero's `+0x3C8`, the fame level `+0x3D0`, ...), so closed
    they hold the layout's samples ("Name", "XP:0-0", "HP:100"). Finding the instance among
    `CGameUI`'s fields to call that update crashed the game before the search ended, twice (first
    with a `break` out of a `__try`, then with the reads inside it). The column now shows its
    frame and labels with no numbers. Next: read the hero's fields as that update does, without
    calling it. **[the runs]**
- **The icon's background** (`sub_82368980`, its table at `0x8339FB08`): red for a weapon, armour
  or trinket the hero cannot equip (CEquipment's virtual `+0x164`, `(character, 1)`), gold for
  UNIQUE (the 360 names `traycolor_gold`, which its imageset lacks; yellow stands in), blue for
  MAGIC, green for RANDOMMAGIC; set items not done.
- **The comparison arrow**: the 360 compares the item's value with the one equipped in its place
  (up, down, or none if equal); here weapons by PC's DPS and armour by its value (below) against
  `getComparisonItems`' first item.
- **Identified names** in the 360's grey (`FFCCCACF`, a string of its image next to the tabs').

**The doll, the Info card, PC's HUD (2026-10-16, the user's "metele").** **[the runs:
`~/torchlight-gog/screens/t14-unclip.png`, `t20.png`, `t21.png`, `t21-armor.png`,
`t22-compare.png`]**
- **The character column's numbers now show.** The windows that drew were those the layout marks
  `ClippedByParent False`; the others were clipped away. Every window of the column is now
  unclipped, and they all draw. A likely cause, seen later: CEGUI keeps each window's screen rect
  and does not drop the children's when a frame is moved under another parent
  (`addChildWindow`); their pixel rects came back as at the layout's load (the column's
  `CharacterViewport` at (22, 236) while its `TopFrame` was at (325, 123)).
  `Window::notifyScreenAreaChanged` (exported, recursive) drops them; the proxy calls it before
  reading a rect.
- **The fame title**, as PC's stats menu picks it (GOG `0x5b146e`): the game data's singleton
  (`0x524d40`) holds the titles (VC9 wstrings of 28 bytes from `+0xc8`, their count `+0xcc`, a
  bound `+0xd0`), the fame level minus one, at most the last ("Unrecognized" at fame level 1).
  The name is PC's `+0x420` ("the test character": PC's own name for a character the console was
  used on).
- **The 3D doll.** PC's own inventory shows the hero in 3D. `CInventoryMenu` (CGameUI's `+0x2cc`,
  vtable `0xab73e0`) keeps:
  - the wardrobe's scene manager (`+0x7b04`) and its camera, "WardrobeCam" (`+0x7b08`);
  - the render window (`+0x7b0c`).

  On showing (its virtual `+0x1c`, GOG `0x57da90`) PC:
  - adds a viewport (RenderTarget's virtual `+0x20`, `addViewport(camera, Z 3, 0, 0, 1, 1)`) and
    keeps it at `+0x7b10`;
  - sizes it with `Viewport::setDimensions` (166x192 scaled);
  - clears it black every frame and sets the camera's aspect to the viewport's.

  On hiding (its update, `0x580f80`) it removes the viewport (virtual `+0x2c`,
  `removeViewport(3)`) and zeroes `+0x7b10`.

  The 360 does the same with its own camera ("WardrobeCamXenon", `sub_8233C534`) on its
  `CharacterViewport` window's rect (`sub_8235AA28`: the viewport's relative rect from the window,
  cleared black, the aspect set, `setCamera`).

  Here PC's viewport is made the same way and placed on the 360's window. The game draws CEGUI
  after every viewport, so the column's texts and frames show over the doll, as on the 360. PC's
  camera frames the doll for its small viewport, so its vertical field of view is widened while
  the screen is open (`Frustum::setFOVy`, the tangent of half the angle times 1.6) and given back
  on closing. The doll follows the equipment (the dagger equipped with A showed in its hand).
- **The Info card** is PC's equipment tooltip: the 360's card in its screenshots ("Rifle / Small
  Cannon / 28 damage per second / (2-handed) / Physical Damage") is the same as PC's.
  - The call: `CGameUI::showEquipmentTooltip` *(OT)* (GOG `0x541e80`, `ret 0x14`) takes the
    character, the item, the tooltip, and the two cards it is compared with.
  - The tooltips: CGameUI's `+0x2ac` (then `+0x2b0` and `+0x2b4` for the comparison). The root
    window is the tooltip's `+0x14`.
  - The cache: PC rebuilds a card only when the item's GUID changes. The tooltip keeps it at
    `+0x8` and `+0xc` (GOG `0x54281f`), so the proxy clears it first. Without that, an item
    identified with A kept its "Unidentified" card.
  - Placement: the card is moved onto the GUI sheet, to the right of the bag's list over the
    third column, its top at the selected row's. As on the 360 it is not kept inside the column
    (it may be wider) nor inside the frame; only the screen's bottom keeps it up.
  - An unidentified item shows PC's "Unidentified items cannot be used - use an identify scroll
    to reveal enchantments".
- **Item compare** (the bar's `ToggleItemCompare`, beside the d-pad's left-right image). The 360
  writes "enable item compare" or "disable item compare" by a flag (`sub_8236C8D0`, the global's
  `+0x14c4`); the d-pad left or right turns it. (The d-pad does not toggle the Info card, as read
  first: the card is always there.) On, the item's card shows what it would replace:
  `getComparisonItems`, then PC's comparison cards, as PC's hover fills them (GOG `0x55eb18`).
  - Where the 360 puts them **[seen]** (the recomp's screenshots, 1920x1080, taken by the mods
    agent: `~/torchlight-gog/screens/ref/x360-ref-en-compare-{weapon,armor}[-off].png`): the
    equipped item's card to the left of the list, over the character's column, its right edge at
    the list's left edge (x 762 of 1920) and its top at the selected row's; the chest armour's,
    on the last row, runs past the frame's bottom. Off, only the item's card is shown. The proxy
    does the same; with two equipped items (both hands) the second goes under the first, a case
    the screenshots do not show **[inferred]**.
  - A run (`t23-compare.png`, `t23-armor.png`): the dagger with a suffix against both
    equipped blades, "(equipped-right hand) <blade>" and "(equipped-left hand) <blade>";
    the spare shoes against "(equipped) <slot> <shoes>"; the mantle, with no
    shoulders equipped, with no comparison card.
- **Other tabs' comparison** (a run with `SPAWNCLASS RINGS_SPAWN`, `SHIELDS_SPAWN` added to the varied
  save; `t24-ring.png`, `t25-shield.png`): a ring against the equipped one, "(equipped-right hand)
  trinket / prefixed ring / +2 Mana"; a shield against the left hand's common blade
  (what PC's `getComparisonItems` gives). A spell has no comparison (A "Learn"). Necklaces did not
  spawn from `NECKLACES_SPAWN`; not checked.
- **The rows' full names.** The 360's rows write the full name ("prefixed shoes"): PC's
  tooltip's name line comes from GOG `0x4b3fe0` (thiscall on the item, the `std::wstring` returned
  into its first argument, then 0; `ret 0x8`; called at `0x54289a`), used for identified
  equipment; its neighbour `0x4b4470`, `CEquipment::getFullItemName` *(OT)*, builds the line above
  ("rare sword", "unidentified trinket"). A long name is cut by the row, as on the 360 ("A
  Mysterious Polear").
- **A in the pet's bag.** The 360's A (`sub_8236AA48`) acts for the bag's owner (the menu's
  `+0x174`): identify, use, equip or take off. Here the same functions as the hero's, for the pet:
  equip into the pet's first free place, take off into its bag, only what PC's `canEquip` lets the
  pet wear (the pet uses nothing). A run (`t28-pet2.png`): a ring swapped to the pet with X, then
  A: "prefixed ring", Equipped, its A "Unequip"; A again took it off. Something the pet cannot
  wear was not tried.
- **The name's colour.** The 360 takes it from the item (`sub_822BBCA8`: unidentified a fixed red,
  else by the item's kind; "prefixed shoes" in green); PC's tooltip by the same kinds
  (GOG `0x5431a5`-`0x5438d9`, for the hero): its colour table (the singleton `0x524d40`, wstrings
  `0x1c` apart) at `+0x55c` if `0x4818f0`; the default unless the virtual `+0x154` or isa `0xa0`;
  else `+0x540` if the item's "SET" (`0x4b28f0`) is not empty, `+0x524` unique (isa `0x36`),
  `+0x508` rare (isa `0x37`), `+0x4ec` otherwise. The rows call the same; the default keeps the
  360's grey. A run (`t27-weap.png`, `t27-jewel.png`): the dagger with a suffix in PC's
  rare blue (`FFC0C0FF`), the unique sword in gold, two prefixed rings in green.
- **A on an unidentified item** reads "Identify (N)", N the identify scrolls in the bag (the 360's
  screenshot: "A Identify(11)"). The bar's buttons follow each other's text, as there: Y after
  A's text, X after Y's (`Font::getTextExtent`), 10 pixels apart at 768 rows.
- **Armour on the rows.** The 360 (`sub_82368980`) writes its "armor" (the tooltip's word), a
  space and the value, and compares it as it compares DPS. PC's tooltip reads an ARMOR's (unit
  type 13) value from the item's `+0x2ec` itself (GOG `0x543d2f`; there is no getter). A run:
  "armor 2" on shoes and the belt.
- **PC's interface hidden while the screen is open.** The GUI sheet's other children ("Ingame UI
  Sheet", "Top UI Sheet": the portrait, the skill bar, the orbs) are hidden, and shown again on
  closing (a run: back as before). The minimap is not CEGUI and stays; the 360 shows its own
  minimap there too.

- **The "Effects:" line (2026-10-16). [read; seen]** The 360's row writes "Effects:" and a count
  (`sub_82368980`, the string at `0x820d0ef0`, in the row's `Enchantments` window; its screenshot:
  "prefixed shoes / armor 2 / Effects:2") for an identified item with any, not a CONSUMABLE
  (1), and not at a GAMBLER (127). The count (`sub_822AFA10`, recursive): the item's elemental
  damage bonuses above 0, plus the sizes of the effect manager's three lists (`+0x17c`, then
  `+0x1c`, `+0x2c`, `+0x3c`), plus the same count for each socketed item. In GOG the fields are
  where `CEquipment::getDamageBonus` *(OT)* (GOG `0x4b84d0`: the effect types 0x34 and 0xa of the
  socketed items' managers) reads them: the damage types a `std::vector` at `+0x30c` (begin,
  end), their bonuses at `+0x324`, the socketed items a list at `+0x3a0` (data, size); the effect
  manager at `+0x17c` (`hasEffects`, `0x4b3c20`). (Two earlier tries, the 360's offsets moved
  0x20 and a search for two vectors 0x10 apart, failed: GOG's vectors are 0x18 apart.) A run
  (`~/torchlight-gog/screens/t40-effects.png`): "Effects:1" in the 360's cyan on the dagger with a suffix (one affix); counted 9 on the unique sword, 6 on the mantle, 3 on the
  belt (unidentified, so not shown), 0 on plain items.

**Not done yet** (noted for later):
- the housing's LT/RT tabs (Quest, Stats);
- "pet commands";
- the sockets' images (`socket0_`, `socket1_`), set items;
- X (sell) in a shop, the transmuter;
- the identify scrolls' window;
- the 360's own HUD: done, section 11b.

## 11b. The 360's HUD in play (2026-10-16)

The user's order: the 360's interface, PC's hidden. The 360's references: the mods session's
captures in play (`~/torchlight-gog/screens/ref/x360-ref-en-hud-still.png`, `-pet.png`; the mines' third floor): the orbs top left and right with a potion and its count over LB and RB, the
level's name top right, the skill bar at the bottom (Y, LT, RT, B), the experience bar under it,
the pet's portrait and bars bottom left. **[the screenshots; the runs]**

- **The same windows as PC's.** The 360's `CGameUI` finds its HUD's windows by PC's names (its
  init `sub_823392E8` names `PlayerHealthBar`, `POTIONCOUNT`, `ExperienceBar`, `TargetHealth`...).
  Of the 360 `hud.layout`'s 41 named windows, 32 are in PC's `bottomhud.layout` (86): the bars, the
  skill slots and texts, the target's, the messages, the level's name; the 360's own: `POTIONCOUNT`,
  `MANACOUNT`, `SkillSet1`/`2`, `AvailableSkillsIcon`, `TargetInteractHover`.
- **So PC keeps its HUD and the 360's shows its values.** The proxy loads the 360's `hud.layout`
  and `pethud.layout` on the GUI sheet once PC's HUD exists, and makes PC's bottom HUD and pet HUD
  transparent (`Alpha` 0 on `PlayerHealthRoot` and `PetHud`; PC shows them again every frame, so
  hiding them does not last). Every 50 ms, on the pad's poll (the game's thread):
  - **The bars** as PC leaves its own: PC shrinks a bar (a clipping window) from its top (from
    its right for experience) and moves its image (`...Sub`) the other way so it stays put (a
    run: `PlayerHealthBar {472.5, 23.5, 525.9, 130.8}`, its Sub `{0, -8.1, 53.4, 107.2}`, 0.93 of
    the health). The fraction, the bar's height (width) over its image's, is given to the 360's
    bar and image of the same name: health, mana, experience, the pet's health and mana.
  - **The texts and their visibility** as PC's: `LevelName`, `LargeMessageText`,
    `SmallMessageText`, the pet's `FleeingText`; the pet's HUD shown when PC's is.
  - **The pad's slots:** LB, RB, B, Y, LT, RT press PC's hotbar keys 1 to 6 (section 9), so the
    360's slots show PC's: `POTIONCOUNT` and `MANACOUNT` the texts PC writes on its slots 1 and 2,
    and the icons of PC's `SkillSlot1`-`6` in the 360's LB, RB, B (`SkillSlot5`), Y (4), LT (6),
    RT (7) (a `StaticImage` of ours made in each with `WindowManager::createWindow`, given the
    image of PC's icon). The 360's second set (`SkillSet2`) is hidden.
  - Hidden as not done: `TargetInteractHover` (the 360's "A Interact"), the skill slots' texts.
- **Runs** (`~/torchlight-gog/screens/t31-hud.png`, `t32-lb.png`, `t32-inv-hud.png`): the HUD
  where the 360's is, the health and mana potions with "2" over LB and RB, LB drank one ("1"),
  the experience bar at PC's fraction, "the mines' first floor" top right, the pet bottom left with
  its bars and stances; with the inventory open, the orbs behind the frame as on the 360.
- **The target's health, the 360's rule (2026-10-16). [read; seen]** The 360's HUD update
  (`sub_821A83C8`) takes, as PC's (GOG `0x558750`) does: the hero's target (`+0x284`; PC's `+0x290`)
  while an attack button is held, else the unit under the cursor (its `CGameUI`'s `+0x48`; PC's
  `+0x2c`), and then, which PC does not have, the player's virtual `+0x1cc` (`0x82200C78`: its
  `+0x9f8`), the unit its attack (`sub_822919A8`) sets through the virtual `+0x1c8`
  (`sub_822D7C08`) for 3.0 seconds (the timer at `+0xa00`, run down in `sub_821DC980`). So the 360
  shows the unit the hero attacks, not X's candidate. Two tries:
  (1) the unit put in PC's mouse-over pointer (`CGameUI`'s `+0x2c`, a safe pointer, with PC's
  assignment `0x4944e0`) so that PC draws its bar: PC clears it every frame (logged 0 on every
  tick after the write), no bar; (2) the 360's HUD filled from the unit, as PC fills its own: the
  name (the unit's `std::wstring` at `+0x420`, PC's `0x5589a5`), "Level" and the level (`+0xf0`,
  PC's `0x5588e8`), the bar at the health (`+0x394`, a float; PC's `0x487e00`) over the maximum
  (`0x4901e0`); the unit kept 3 s from the hero's last attack on it (the hero's target while
  `performingAttackLoose`), in a safe pointer of ours, so that it goes when the unit dies. A run
  (`SPAWNCLASS MINE_CHAMPIONS,1,1`, `~/torchlight-gog/screens/t45-target.png`): "Level 1 /
  Carnivorous Gel" over a nearly empty bar at the top, as the 360's, gone when it died.
  `TargetDescription` (PC's champion line, `0x4854a0`) is hidden: not done.
- **The user's run (2026-10-16) and its fixes. [seen]**
  - *The target's bar never came from the 360's HUD:* the hero's target and `performingAttackLoose`
    were not both set at a 50 ms tick (four X attacks, no "target shown"); the bar seen was PC's
    own, for the unit under the mouse, in PC's font. Now X's attack (`proxy/pad.c`) hands its
    target to the HUD at once, as the 360's attack sets its +0x1c8; PC's `TargetHealth` is made
    transparent; and the unit under the mouse (PC's `CGameUI` `+0x2c`) comes first, as the 360's
    cursor unit does. A run: "Level 1 / Shadow Recluse" with its health.
  - *The HUD stayed on the title screen after "Exit to Title":* without a hero the pad's poll
    returned before the HUD's tick. Now the tick runs there too and hides the 360's HUD and pet
    HUD while there is no hero or no PC `PlayerHealthRoot`; PC's windows are searched again on
    the way back and made transparent again if new. The tick is guarded (`__try`): while a level
    is left or loaded the level's automap pointer is not valid (logged five times at the minimap
    before it got its own guard; one run closed the game on "Continue" before the guard). Runs:
    the title with no HUD; back in play twice with the 360's HUD and PC's hidden.
  - *The minimap:* PC's, top right; the 360's bottom right. The 360's `CAutomap::setVisible`
    (`sub_822E6090`), out of its fullscreen mode, sets its viewport to a square of 200 (its
    `+0x50`, from the constructor `sub_822E3A30`) times the screen's scale, 50 from the right
    edge and 70 from the bottom (the constants at `0x820aa388`, `0x820aa570`); its captures:
    187 x 187 at 1045, 466 in 1280 x 720 (scale 720/768). PC's (GOG `0x4ea6b0`, called on
    toggling) keeps it at the level's `+0x160` (the viewport `+0x28`, shown `+0x3c`, fullscreen
    `+0x65`); its corner viewport is set every tick to the 360's place with
    `Viewport::setDimensions` (a run: bottom right, its frame with it).
  - *Still open:* the potions' icons are PC's (a diagonal vial; the 360's is upright, its own
    item icons, not converted); the menus' fonts the user saw stretched (the 360's fonts are
    auto-scaled from 1024 x 768; the inventory's text matches the 360's screenshots in width);
    the pet's column (below).
- **The pet's bag shows the pet (the 360, read, not done).** On moving between the bags
  (`sub_823702E0`) the 360 calls `sub_8235AA28` for its two character columns (`+0x10c` the
  hero's, `+0x110` the pet's), active for the focused bag: the column's root (`+0x14`) shown, its
  character's model (`+0x18`, its `+0x1b0`, the virtual `+0x24`) shown, the doll's viewport placed
  on the column's `CharacterViewport` (`+0x8c`) and its camera's aspect set. So with the pet's bag
  focused the left column is the pet's, with the pet in 3D.
- **Not done:** the pet's stance buttons (the 360's three images, not PC's state);
  `AvailableSkillsIcon`; the HUD's own position checked against the 360's (by eye, the same).

## 12. Pad prompts in other pads' style (2026-10-16)

The user's order: the same PlayStation and Nintendo icons as the recomp, its code and icons copied
here (no dependency between the repositories). **[read: the recomp's branch; the conversion]**

- **The source:** the recomp's mods session, merged into its develop (`b0cd91a`, "Merge
  feature/pad-prompt-styles into develop"): `src/mods/pad_prompts.cpp` (`PromptIcons`, `FitIcon`,
  `PatchPromptTexture`), `tools/pad_prompts/make_icons.py`, the icons in `data/ui/prompts/<style>/`
  from Kenney's "Input Prompts" 1.5 (CC0).
- **Here:** `tools/pad_prompts.py`, a Python port (pure Python: a PNG reader, a DXT1/3/5 and
  A8R8G8B8 DDS decoder, the A8R8G8B8 encoder, the same table and fitting); the 54 icons in
  `data/prompts/<folder>/` as PNG (the recomp's DDS decoded; this repository's CI forbids
  `.dds`), with Kenney's license (`data/prompts/LICENSE.txt`).
- **What it replaces** (24 images): `a_button`, `b_button`, `button_x`, `y_button`,
  `left_bumper`, `right_bumper`, `left_trigger`, `right_trigger`, `start_button`, `back_button`;
  the help screen's `controller` (the style's silhouette at 0.8 of its region); the sticks
  (`analog_button_left_lrud` and the like: the plain stick, horizontal, vertical;
  `analog_button_left_down` is drawn as the right stick pressed) and the d-pad (`dpad_up`...,
  `dpad_updown`, `dpad_leftright`). PlayStation by model (`playstation3`, `playstation4`,
  `playstation5`; `playstation` alone is the PS5's): cross, circle, square, triangle, L1, R1, L2,
  R2, start and back as that model draws them (PS3 start/select, PS4 options/share, PS5
  options/create), its controller. Nintendo by position (the 360's A is the bottom button, B), or
  with `--swap-confirm-cancel` the letters swapped (the input swap itself is not done here).
  Every icon keeps a 1-texel transparent border inside its region (the game's windows draw these
  images a little smaller and clip the bottom row).
- **How:** `convert_x360_ui.py PAK OUT --prompts playstation5|nintendo|...` rewrites the converted
  widget textures (each replaced image's region cleared, the icon fitted in, aspect kept; an
  uncompressed A8R8G8B8 DDS); imagesets and layouts untouched. On the user's pak: `ratings` 2
  images, `widgets` 3, `widgets1` 12, `widgets2` 7. Contact sheets:
  `~/torchlight-gog/screens/prompts-x360ui-ps-sheet.png`, `prompts-x360ui-nin-sheet.png`. The
  style is chosen at conversion (one OUT folder per style: `x360ui-ps`, `x360ui-nin`; `out=` in
  `x360_ui.ini` picks it). In a run with `x360ui-ps` (`~/torchlight-gog/screens/t30-ps5.png`): ✕
  Equip, △ Drop, □ Swap, R1 by the filters, the d-pad's left and right lit by item compare, ○
  Close.
- **Not covered:** texts that name buttons ("Press A to identify", "item comparison: off").

## 13. A varied test save, made with the game's console (2026-10-16)

The user asked for a save with variety to check the rows (unidentified items, armour, qualities,
something the hero cannot equip) and identifying with A. **[the runs]**

- **The game's console:** GOG PC keeps Runic's developer console (`CConsole`, its `HELP` lists
  ITEM, SPAWNCLASS, MONEY, LEVELUP, IDENTIFYALL, ...). Typed through `send_input` it lost keys, so
  the proxy runs commands itself: `console=CMD;CMD...` in `x360_ui.ini` (test runs only) gives each
  to `CConsole::executeCommandLine` *(OT)* (GOG `0x56cd00`, `ret 0x1c`: a VC9 `std::wstring` by
  value, built with msvcp90's own constructor), the console being `CGameUI`'s `+0x13EC`, once, at
  the first opening of the inventory (run while the level loaded, the items never appeared). The
  proxy's own `X360PICKUP` then picks up what lies around (PC's click order, as A does, one item
  every 1.2 s).
- **The save:** `~/torchlight-gog/save-backups/variety/` (the mine save plus `SPAWNCLASS
  NEWBIE_CHEST_WEAPON,2,1`, `NEWBIE_CHEST_ARMOR,2,1`, `SWORDS_MAGIC_SPAWN,2,1`, `ARMOR_MAGIC,2,1`,
  `ITEM <a unique sword's name>,1` (unique, level 15), `ITEM <the identify scroll's name>,3`,
  `ITEM <the healing spell's name>,1`,
  `MONEY 500`), written by "Exit to Title". Its bag: two daggers unidentified, the unique sword unidentified, a common blade and the second blade equipped, a rifle, boots, unidentified
  shoulder armour and belt, the healing spell, identify scrolls, 600 gold.
- **Checked in the run** (`~/torchlight-gog/screens/x360-gog-identify-compare.png`): unidentified
  rows in red with "???" and "Press A to identify" and a red background; A identified a dagger (an identify scroll spent): DPS 35, price 600, a blue background (magic), the green up
  arrow (35 against the equipped 33), its A now "Equip". "12/63" and the gold 600.
- **Item names** the console takes are the units' `NAME` (`media/units/items/*/*.DAT` in PC's pak);
  random items come from spawn classes (`media/spawnclasses/`, their `NAME`).

## 14. The 360 catalog, screen by screen (2026-10-17)

The user's plan for the night: every screen of the 360 catalog
(`~/torchlight-gog/screens/ref/catalog/`, made by the recomp's agent: `menus/`, `town/`,
`in-game/`, each a 1920x1080 PNG and a JSON dump of the CEGUI tree, its `README.md` gives the
format) in the GOG game, pixel perfect or very close. The rule, verbatim: "esto es portear de 360
a pc, nada de inventar código": everything comes from the 360's layouts, images, texts and code;
what is not known is looked up in the recomp, not guessed. Each screen's GOG capture goes to the
recomp's agent, who answers with the differences.

**The tools:**
- **PC's dump** (`proxy/ui_dump.c`): when `x360_dump.req` appears in the game folder, the proxy
  writes `x360_dump_NAME.json` (NAME: the request's first line) in the catalog's format
  ("torchlight-ui-dump 1"), from the HUD tick (the game's thread). It reads CEGUI only through
  its exports, checked in GOG's `CEGUIBase.dll`: `Window::getUnclippedPixelRect` (`rect`) and
  `getPixelRect` (`clipped`) return a `Rect` laid out top, bottom, left, right (the constructor
  `Rect(left, top, right, bottom)` at `0x1008bdf0` stores its second argument at `+0`, the fourth
  at `+4`, the first at `+8`, the third at `+0xC`; `getWidth`, `0x10001aa0`, is `[+0xC] - [+8]`);
  `isVisible(true)` is the own flag (`visible`), `isVisible(false)` with the parents' (`shown`).
  The font is the window's font in use (`getFont(true)`) by its `Name` property; the images are
  the `Image`, `NormalImage`, `HoverImage`, `PushedImage` and `DisabledImage` properties. The
  base name drops the proxy's layout prefixes (`x360inv_`, `x360hud_`...) or a layout instance
  number, as the catalog's.
- **The comparison** (`tools/compare_ui.py REF.json PC.json [--under NAME]`): shown windows only,
  matched by base name and their parent's; the 1080p rectangle (1 px), text, font and imageset
  (without the converter's `X360_`), images, effective alpha.

**The target's bar** (`in-game/01` to `03`): the 360's HUD update (`sub_821A83C8`) fills, for the
unit it shows: the name (its `+0x404`) in `TargetName`, white, or, for a champion (its `+0x63A`),
in the colour of its constants (the catalog: `FFFFE03D`); "Level" and the level in `TargetLevel`;
the unit's data's `DESCRIPTION` (its `+0x178`, the key at `0x820bcce0`; "Undead - Fast", none for
an NPC) in `TargetDescription`; and it shows nothing for a mimic still in its `MIMICIDLE`
animation (type `0xA7`). PC's update (GOG `0x558750`) has the same pieces: the champion flag at
`+0x64E` with the colour (1, 0.88, 0.24) (the floats at `0xab2150`, `0xab2154`: `E0`, `3D`, the
360's), the description by `0x4854a0` (thiscall on the unit, a `std::wstring` out: the data at
`+0x178`, the key at `0xd5d9a4`), the mimic test (`0x480760` with `0xA7`, then `0x482740` with
"MIMICIDLE"). The proxy now fills the 360's three lines with those.

**The inventory against the catalog** (`menus/08`, `09`; the recomp agent's review: "muy cerca,
OK"):
- **Icons** (the "stretched" potions the user saw): the 360 sizes an icon window from its image
  (CEGUI's `Image::getWidth`/`getHeight`). A row (`sub_82368980`, `0x82368D14`): the image's W x H
  times `min(frame height / W, frame width / H)` (the frame: the row's `QualityBk`, 54 x 80), halved
  for a unit of type `0x78`, centred. The hotbar (`sub_821F42A8`, `0x821F4A5C`): as tall as the
  slot (36), as wide as the image's proportions give, centred across (a potion 24 x 36).
- **Columns**: the bag at 299 and the pet's at 602 (`sub_823788D8`'s constants 300 and 603 place
  the column roots; the catalog's frames give 299 and 602). Only the focused column has its
  `_rollover` images (frame, border, its filter's tab) and RB. A column that is not the
  player's (the 360's ISA 134 test) dims to 0.25 the tabs of the filters it has nothing in, but
  the focused column's own (`sub_823654B8`, `0x82365948`).
- **The bar's buttons** (`sub_8236E888`): each takes its icon plus its text (A and X also 2.5); a
  gap of `max(0, (0.9 x tray width - sum) x 0.15)` after each with a text; the group centred at
  `gap/2 - sum/2 + 5`, in the order A, Y, X (720-row pixels, scaled by height / 720 here).
- **The page tabs**: "Inventory", "Stats", "Skills", "Quest" round, LT to the left and RT to the
  right (`sub_82377990`, `sub_82377928`). The trigger buttons sway: `sub_82376728` registers
  `lefttriggerbutton` and `righttriggerbutton` (speeds 1 and -1) as animated windows
  (`sub_82344BF0`: scale 20, limit 5), `sub_821A41A0` moves t by speed x seconds, x = place +
  20 t (720-row pixels), and past |t| = 0.25 sets t = speed x 0.25 and turns the speed round.
- **The pet's commands** (Start; `CMessagePetCommandsXenon` in `messagehousing_xenon`): the stance
  (GOG `+0x65c`: 0 passive, 1 aggressive, 2 defensive) turned right 0 -> 1 -> 2 -> 0 and left
  back by the stick or d-pad (`sub_8237B080`), then GOG `0x4949d0` (PC's own stance buttons,
  `0x54ddfb`); Y: PC's departure test (the level's `+0x15c` `+0x89`, else "Your pet cannot depart
  from here!") and `CCharacter::sendToTown` *(OT)* (GOG `0x49a390`). The HUD shows only the
  current stance's button (the others are `pethud_base`'s grey ones).

**The target's bar size** (`sub_821A83C8`, `0x821A95E4`): `TargetHealthBar` at (1, 1) in `i_780`,
the health fraction (at most 1) times 220 (rounded in 720-row pixels) wide, 84 high, clipped by
its parent.

**The Stats page** (RT; `CStatsMenuXenon`, `menus/10`): `statsmenu_xenon` at 299 and
`journalmenu_xenon` at 602 beside the character's column (`sub_82385738`, its constants 299 and
602). Its windows by the table at `0x833fa020`; the fill `sub_82384738` with the GOG getters:
strength `0x487ec0` (base `+0x3ac`), dexterity `0x487e60` (`+0x3a8`), magic `0x487d70` (`+0x3b4`),
defence `0x487d00` (`+0x3b0`); damage minimum `0x499e10` and maximum `0x492f60` with the same
arguments as the 360's (`(1,0,x)` melee, `(0,0,x)` ranged, `(0,1,x)`/`(1,1,x)` magic); armour
`0x4900d0`, its half `0x4901b0`, its base `0x490150`; resistances `0x48ffe0` (poison 5, fire 2,
ice 3, electric 4); the points `+0x3dc`. The colours FFc0c0ff / FFFF0000 / FFFFFFFF by the value
against the base (`sub_82384238`). Up/down choose the stat (0 to 3, no wrap), A adds a pending
point, Y takes one back; the pending points are spent when the page is left (PC's own +1
functions `0x488450`, `0x488470`, `0x4884b0`, `0x488490`; the -1 ones `0x4884d0`.. are used to
read the values with them added, as the 360's `sub_82383ff8`/`sub_82384118`). The journal: PC's
statistics at `+0x740` (PC's own journal `0x586c00`, the same 18 labels), the time played the float
at `+0x2c8`, as 36 StaticTexts of the 360's sizes and colours.

**The Skills page** (`menus/11`, the recomp agent's same-character reference `test/04`; "muy
cerca"): `skillsmenu_xenon` filled as `sub_8235BE88` from the player's skill manager (GOG's CSkill
has the 360's offsets: tab `+0xdc`, row `+0xd8`, column `+0xd4`, rank `+0xa4`, effective rank
`+0xa8` from `0x5d7d40`; the manager's list `+0x3c`, count `+0x40`, the 360's `+0x2c`/`+0x30`); icons
by name from the itemicons imagesets; the selected skill as `sub_8235D558`: name `0x5d59b0`, kind
`0x5d68e0`, the rank's effects `0x5d8c60` (its `0x5e0fc0` writes "Inflicts .. of Weapon DPS"),
Next Level with the next rank's and its required level (`0x5d5c00`); the help box as
`sub_82381D18`; moves as `sub_8235E3A0`..`E6D0` with the spells' row (`getKnownSpell` *(OT)*,
`0x484ff0`); A learns as `sub_8235E9D0` through PC's `0x488430` and `0x5de550`. The texts carry the
360's colour marks (`|cAARRGGBB` .. `|u`, the data's own); PC's CEGUI writes them as they are, so a
whole marked text becomes plain text and colour, and a description gets one StaticText per
paragraph in its colour. **Not done:** X, mapping a skill to LT/RT/Y/B (read below, not
written); the 360's requirement line ("Right hand requires : melee Weapon", built by
`sub_823B07C8` from its own strings, which PC does not have).
**Read 2026-10-22 (not written):** the 360's description (`sub_823B07C8`) is, in order, a
cooldown line (`|cFF9180FF` "Cooldown" ": " the value as `%.1f` " " "seconds" `|u \n`), a mana
cost line (`\n|cFF9180FF` "Mana Cost" ": " the value `|u\n`), the requirement line in
`|cFF9180FF` .. `|u \n`, then the effects (`sub_823C0AE0`, PC's `0x5d8c60` here). The
requirement (`sub_823B0230`, from the skill's `+92`, GOG's CSkill `+0x5c`: the right hand's unit
type `+8`, the left's `+0xc`, dual wield `+0x10`, as PC's own test at `0x5d5760`): "Dual wield
only" and a new line when dual; then, when both hands ask the same type, "Right and left hands
require " ": " the type's name " " "Weapons", else "Right hand requires " ": " the name " "
"Weapon"; nothing when neither asks. The type's name (`sub_823AFDB0`, by the type hierarchy,
PC's `0x5fe190` on a unit's `+0x60`): MELEE (60) is "single handed melee" when ONE HANDED (9),
"two-handed melee" when TWO HANDED (10), else "melee"; RANGED WEAPON (35) or
ITEMCATEGORYRANGED (163) the same with "ranged"; else "any". The strings are the 360's
(`0x820D4420`-`0x820D45D8`, `0x820BE144`, `0x820D0BE0`); PC has none of them. The cooldown and
mana values' sources were not read yet (`sub_823A62C8`, the skill's fields).

**The Skills page's mapping (X), read and written 2026-10-21, not run yet.** The page's input,
`sub_823827F8` (`GamepadButtonDown`), switches on Runic's button code (section 10b); a second
handler, `sub_82382C58`, takes `GamepadButtonUp` (both subscribed in `sub_82381268`). **[read]**
- X (7): with the overlay `+1952` (`skillsmap_xenon.layout`) hidden, and a selected skill
  (`sub_8235E830`) not passive (`+56 != 4`) with an effective rank (`sub_823AE2A0`, then
  `+168 > 0`), it hides the help box `+1956` and opens the mapping, `sub_823825F0(1)`. With the
  overlay shown, X closes it, `sub_823825F0(0)`, and `sub_8235EBB8` redraws the slot icons from
  the player's hotkey GUIDs (ten 8-byte slots from `+2112`).
- B (6), Y (8), RT (9), LT (11) pressed, with the overlay shown: `sub_8235EAD8` sets alphas on
  the tree's `PressButtonLT/RT/Y/B` (named in `sub_8235AEF0`; found only in
  `charskillsmenu_xenon.layout`, which no path in the image names, so on this page the lookups
  are likely empty), then `sub_82381D18` redraws the help box. With it hidden: B closes the page,
  LT/RT change tabs, Y does nothing; the sticks move and A learns only with it hidden.
- **Released** (`sub_82382C58`), with the overlay shown: the slot is B 4, Y 3, LT 5, RT 6, or
  B 7, Y 2, LT 8, RT 9 when the flag `[[menu+168]+6032]+5220` is set (the second set).
  `sub_82382720(menu, slot)` checks the skill as X does plus `+67 == 0` and `+69` set, writes its
  GUID (`+384`) into the slot and, if it is not -1, clears the slot's item (`+2192`), plays the
  sound 30 and redraws; on success the overlay closes, else it stays.
- PC's `0x540601` (the drop of a skill on a hotbar slot) is the same function: the same tests
  (`0x5d7d40`, `+0xa8`, `+0x43`, `+0x45`), `0x40c8f0` (thiscall player, slot, GUID lo, hi; it
  clears the item slot the same way) and the sound `0x1e` through `0x5366f0` with no node. PC's
  `SkillSlotN` has the ID N-1 (`0x562953`), the slot index.

**Written** (`skills_pad`, `skillmap_pad` in `proxy/x360_menu.c`): X opens `skillsmap_xenon` over
the page for a skill that passes, with `MapSkillSet1` shown and `MapSkillSet2` hidden (this port
has one set). Its `MapSkillSlot4..7` get the HUD's icons, as the page's `SkillSet1` does: that
fill is ours, the 360's fill of these slots was not read. B, Y, LT and RT on their release call
`0x40c8f0` with the slot the pad fires for that button (`pad.c`: B 2, Y 3, LT 4, RT 5, PC's keys
3-6), not the 360's numbers, so that the button then casts the skill. Then the sound 30 and the
overlay closes; X, Start or Back close it too. To check in the QA: the overlay's place, and that
the skill on B/Y/LT/RT fires from that button in play.

**PC's tips as the 360's (written 2026-10-21, not run yet).** Both builds have a `CTipMenu`:
the 360's loads `media/ui/tipmenu.uilayout` (`sub_823587C8`, which finds `Title` and `Dialog`
and subscribes `sub_823589A0` to `GamepadButtonDown`). **[read]** Its pad: B (6) closes (the
menu's virtual +28); Y (8) calls the settings' `sub_821FA030(.., 0)` (the tips off), saves them
and closes; the other buttons do nothing. The 360's layout shows B "Close" and Y "Disable Tips"
(its own texts). PC's `tipmenu.layout` has `Frame`, `Title`, `Dialog`, `Ok` and the checkbox
`ShowTips` (section 8). **Written** (`tip_tick`, from the frame listener): while PC's tip is
open (its `Frame` shown and under the sheet, found by `ShowTips`), the 360's is shown over it
with PC's `Title` and `Dialog`, and PC's is made transparent (as the NPC dialog). B presses PC's
`Ok`; Y presses `ShowTips` when it is selected, then `Ok`, so PC's own controls turn the option
off. No catalog capture has a tip, so the QA compares it by eye.

**PC's modal as the 360's (written 2026-10-21, not run yet).** The fishing's result ("Fishing",
"You caught …", Ok) is PC's `modalmenu.layout`, the same names as the 360's (`Title`, `Dialog`,
`Ok`, `Accept`, `Decline`). The 360's `CModalMenu` (loaded by `sub_823532D0`; pad
`sub_82353758` on `GamepadButtonDown`): A (5) sends the menu's command 9 when its window `+272`
is visible, else 10; B (6) sends 8 (the menu's virtual `+36`). **[read]** **Written**
(`modal_tick`): PC's modal is found as the only PC layout with `Accept`, `Ok` and `Dialog`
(combine, enchant, dialog and quest dialog lack one of them); while its `Frame` is shown, the
360's is over it with PC's texts and buttons shown as PC's. A presses PC's `Accept` when shown,
else `Ok`; B its `Decline` when shown. The 360 sends B's command also with `Ok` alone: what that
does was not read, so B does nothing then. To check in the QA: that A on the fishing result
does not also reach the fishing menu.

**The Quest page** (`menus/12`): `questmenu_xenon` as `CQuestMenuXenon`: the quests of the player's
quest manager (GOG `+0x7b4`, list `+0xc`, count `+0x10`) with a name (`0x59f180`) and details
(`0x5ea120`), six rows with Quest Complete (`isComplete` *(OT)*, `0x5e9200`), the title, details
and reward boxes (`0x5e8750`, `0x5e8730`, `0x5e8770`). **Not done:** the reward items' sockets.

**The pause menu** (`menus/13`, `14`): Start in play opens `optionsmenu` (the 360's COptionsMenu),
its focus in the rollover image, the trial's Unlock Full Game and Gift a Friend hidden; Help opens
`howto_xenon`. **Exit to Title: stopped after two tries.** (1) Clicking PC's own pause button: PC's
`media/ui/optionsmenu.layout` has `ExitGame`, but no window of that name exists while PC's menu is
closed (neither under the GUI sheet nor by name). (2) PC's own `COptionsMenu` (vtable `0xab7d78`,
among CGameUI's members): its `onClick` *(OT)* `0x591050` sets, for function 0
(`LAYOUT_FUNCTION_EXIT_GAME` *(OT)*), its return to the main menu `+0x68` and close request `+0x1a`,
and acts only while open (`+0x18`); opening it with `0x591200` and calling it from the pad's hook
froze the game (the log stops there). Options (the 360's settings screen, `menus/04`),
Achievements and Leaderboards do nothing yet.

**The 360 code's texts come from the user's copy** (2026-10-17): the texts the 360's code (not its
layouts) puts in its screens (page names, the bags' and buttons' names, the filters, stances,
journal labels, "Requires Level", the rewards' labels, the identify hint...) are no longer in the
proxy's source. `tools/convert_x360_ui.py ... --image IMAGE` reads them from the user's decrypted
360 executable (`~/torchlight-rewrite/out/pcre/default.bin`, base `0x82000000`) at the addresses of
its `STRINGS` table (key -> address only) into `OUT/x360_strings.txt`; the proxy loads that file
(`x360_text(key)`, `proxy/cegui_proxy.c`, from the `out=` folder) and shows "" for a key it lacks.
Run the converter with `--image` once to get them. Earlier commits on this branch still carry
those texts as literals; whether to rewrite the branch's history is the user's decision.

**The user's scope (2026-10-17, relayed by the recomp agent):** the title screen, main menu,
options, new and load character, Exit to Title and the pause's Options, Achievements and
Leaderboards stay PC's; the 360's pause and How To Play, already done, stay. Pixel perfect is for
the gameplay screens: the HUD, the inventory and its pages, the town (merchants, stash,
transmuter, enchanter, gems), the NPC dialogs, target bars, item labels and fishing.

**The merchant** (`town/01`, `07`-`20`; `CMerchantMenuXenon`, vtable `0x8200151c`): while PC's own
merchant menu is open the 360's frame shows three columns, the merchant's (a third
`inventorymenu_xenon`, at the frame's left, where the character's column is), the bag and the pet's
bag (`sub_82378D30`: its `+0x10C`, `+0x108`, `+0x110`), the title `title.merchant` for every
merchant, the Gambler's too; no pages beside it (`MenuLeft`/`MenuRight`, the trigger images) and
no pet commands; the merchant's column focused at the start.
- **PC's side.** CGameUI's `+0x2d8` is PC's `CMerchantMenu` (made at `0x562f57`, vtable
  `0xab7b58`): CGameUI sets its owner (the virtual `+0x18`, `0x58ab90`: the merchant at `+0x28`)
  and opens it (`+0x1c` with 1, `0x557d3f`); `+0x30` is its open flag (the virtual `+0x10`). The
  proxy watches it each pad tick (`x360_shop_tick`) and opens the 360's frame; B closes PC's menu
  (`+0x1c` with 0, as CGameUI at `0x558413`) and PC's inventory (`+0x2cc`), which PC opens with the
  merchant's and which closes the merchant's with it.
- **The column's look** (`sub_82367D38`, the catalog): `backroundbase_merchant(_rollover)`,
  `merchant_tab_*`, the name `title.gambler` at the Gambler (ISA 127) or `title.merchant`, no gold;
  the tabs of empty filters at 0.25 (any column not the player's).
- **Prices** (`sub_822BC4B8`, by the item's owner): the merchant's items show what it asks
  (`CEquipment::sellPrice` *(OT)*, GOG `0x4b3a90`, the price PC's purchase checks, `0x54f01f`), the
  bags' what it pays (`buyPrice` *(OT)*, `0x4b3b70`, what PC's sale gives, `0x54ef2f`); none for
  type 103. The plain inventory showed the asked price before (60 for the 360's 19 in `test/02`);
  it shows the paid one now, as the 360.
- **The header and the buttons** (`sub_8236C5C0`, `sub_8236C8D0`, `sub_8236E888`/`E7B0`):
  `buyandsell` shows `act.buy` (`FFFBE469`) in the merchant's column, `act.sell` (`FFFF4800`) in the bags;
  the merchant's column has X `act.buy` only; a bag has X `act.sell`, A `act.identify` with the
  scrolls for an unidentified item, `act.unequip`, `act.equip` for a weapon, armour or trinket (ISA
  8, 13, 39; an owner not of type 28 a trinket only), `act.socket` for a gem (120), else nothing,
  and no Y. A button with no text is hidden and its
  text's width not counted.
- **Input** (`sub_8236A810`, `sub_8236AA48`, `sub_8236AD18`): LB/RB through the three columns,
  round; A does nothing in the merchant's column; Y nothing at a merchant (mode 4); X buys there
  (`sub_8236B578`) or sells from a bag (`sub_8236B7B8`), at once. Buying: refused (the sound 24, the
  hero's voice 49, `sub_82369E28`) when the gold is under the price or the item is of type 103; an
  endless stock item (a stack of 1 with `+0x1ff`, the 360's `+0x1E4`/`+0x1FF`) is copied (GOG
  `0x5ff150` on CGameUI's `+0x1074`, from the item's `+0x178`, as PC's purchase), another taken out;
  into the bag, or back with the voice 47 when it is full; at the Gambler identified (`+0x2fc` and
  `destroyItemText`, the 360's `+0x31C`), the journal's items gambled +1 (`0x4d90f0` with 15) and
  for type 54 the achievement 5 (`0x5f9f90`, `0x5fa360`, `0x5f9e70`); then `giveGold` *(OT)*
  (`0x487f20`) of minus the price and the sound 23. Selling: refused for type 103; the sound 23,
  the paid price given, PC's items-sold count (`0x4d90d0`, which PC's sale keeps), the item into
  the merchant's bag or destroyed when that is full. The sounds are CGameUI's (`+0x13f8`,
  `0x5366f0`) at the hero (`+0x58`), the voices on the hero's emitter (`+0x230`, `0x5368d0`); PC
  uses the same numbers (`0x54ef26`, `0x54e7d7`). Y also refuses to drop type 103 now
  (`sub_82369E80`).
- **The card** is PC's tooltip with the item's owner as its character: for the merchant's items
  the merchant, so PC writes the asked price with its own label (`0x546c9f`: a character other than
  the hero, of type 41), as the 360's card; for the bags the hero, and PC writes the paid one
  (`0x5471bc`, the merchant's menu open).
- **Tested** (GOG, the mine save taken up to town with the mouse, the blacksmith): the screen
  opens with PC's menu, X bought a staff (100 -> 34 gold, the staff in the bag at 17) and sold it
  back (34 -> 51), RB moved through the columns, B closed both menus.
- **Open: the screen's sides.** With PC's merchant and inventory menus both open, PC narrows the
  world's viewport between them every frame (its viewport listener `0x4025d0`, when CGameUI's
  `0x549b70` finds an open menu on each side: `0x541090`, `0x541130` over the menus at CGameUI's
  `+0x16b8`, by their virtuals `+0x8` and `+0xc`), so outside the 360's frame the screen keeps
  PC's last drawn panels. **Stopped after two tries:** (1) closing PC's inventory under the frame
  (its `+0x1c` with 0) closed the merchant's menu too; (2) pointing the import
  `Viewport::setDimensions` at the proxy to keep the main viewport (Z order 0) whole changed
  nothing (the hook was installed; it never saw the narrowed call). Also PC's first-time Tip for
  its inventory shows after closing (PC opened its inventory).

**The stash and the shared stash** (`town/21`-`26`, `30`-`35`; `CStashMenuXenon`, vtable
`0x8200149c`): the merchant's frame again, with the stash in the left column.
- **PC's side.** CGameUI's `+0x2e4` is PC's `CStashMenu` (made at `0x56304c`, vtable `0xab8730`),
  of the merchant's menu's base: owner `+0x28`, open flag `+0x30`. CGameUI gives it the chest the
  hero used (the hero's `+0x290`) and opens it with PC's inventory (`0x5582c3` for the stash,
  `0x55832e` for the shared one). Its inventory (`0x5aac2d`) is the chest's `+0x404`, or, for a
  chest of type 170 (the shared stash), the game's own, `+0x8` of what `0x52dfe0` returns. The
  same tick opens the 360's frame, and the same B closes both menus.
- **The look** (`sub_82383630`, `sub_823832B8`, the catalog): the left column holds the stash, with
  the merchant's images (`backroundbase_merchant`, `merchant_tab_*`). It is titled `title.stash`,
  or `title.sharedstash` for a chest of type 170 (the 360's strings at `0x820D0EA0` and
  `0x820D0EC4`), and that title is the frame's title too. The columns are 303 and 606 apart
  (constants at `0x820aa8bc`), not 299 and 602. No column shows the gold, and no `buyandsell`
  header shows. The stash's tabs are not dimmed: the 360's stash is the hero's own inventory (its
  `+2548`, `sub_8236AD18`), so its column counts as the player's (`sub_823654B8` dims only the
  others).
- **The menu's mode** (its virtual `+0x34`, the field `+268`, set in `sub_82383DC8`): 7 for the
  stash, 8 for the shared one. Mode 4 is the merchant's and mode 5 the transmuter's (type 134 is
  the transmuter, as PC's interaction names show at `0x4f3d34`). An earlier version took mode 5
  for the stash; corrected.
- **The buttons** (`sub_8236C8D0`): modes 7 and 8 get the plain bar. In the bags A is by the item,
  Y is `act.drop` and X is `act.swap`. In the stash's column Y and X are the same, and A is not
  done (the 360 acts for the column's owner, the hero, on an item of another inventory).
- **Input** (`sub_8236AD18`, `sub_8236A810`):
  - X in the stash's column takes the item: to the hero's bag, else the pet's, else to the ground
    at the hero's feet (`sub_8236B8B0`, sound 18 for a bag).
  - X in a bag stores the item in the stash (`sub_8236BA20`, the target the hero's `+2548` for
    mode 7, the game's for 8): sound 18, or sound 24 and the item stays when it does not fit.
    PC's `pickupEquipment` places it, as for the pet's bag.
  - Y drops the item (`sub_82369E80`), in any column.
- **The card** has no price (PC's tooltip, with no merchant menu open).
- **A first showing.** The left column's `HiliteFrame` kept the area it had before its `TopFrame`
  moved into the `ContainerFrame`. It was drawn at the screen's corner, and its texts (the
  count, RB) were clipped, until a row showed in it. The merchant always has rows, so it never
  showed this; the empty stash did. Fix: the column is moved by a pixel and back each time it is
  shown, so CEGUI works the area out again.
- **Each menu its own column.** The left column's filter and rows are kept separately for
  the merchants and for the stashes, as the 360 has two menus. Before this, the stash's last
  filter showed at the next merchant. *Not tested in game yet.*
- **The rows' alpha** (`sub_823652A0`, from `sub_82368980`): the column's selected row is at
  1.0 whether the column is focused or not (the row's item against `sub_82369F60`'s); the
  others are at 0.9 if equipped, else 0.6. Before this, an unfocused column's selected row
  showed at 0.9 or 0.6.
- **Tested** (GOG, three runs; the stash is the chest at the foot of the brazier in the town's
  square, the shared stash the chest before the shed north of it):
  - Both open, titled as the catalog, the counts PC's (`0/42`).
  - X stored the equipped sword and took it back to the bag, in both stashes.
  - LB/RB moved through the columns, and B closed both menus.
  - Captures: `screens/gog/stash-1..6`, `shared-stash-1..6`.
  - Open as at the merchant: PC's panels at the screen's sides, and PC's Tips after closing (its
    inventory's and its stash's).
  - Fourth run (after the alpha and mode corrections): the rows' alpha (the bag's selected row
    at 1 beside the focused stash), the plain bar in the bag (A by the item, Y, X) and Y and X in
    the stash's column (`screens/gog/stash-1`, `stash-bag-1`, `stash-item-1`,
    `shared-stash-1`). A "hang" there (the frame frozen, the log stopped, a core busy) was not
    one: run 6 saw it again at the merchant while the user had switched windows. Without focus
    PC stops drawing and reading the pad; its main thread waits in Ogre's loop (a backtrace:
    `0x40723f` in the top loop, `0x5fdc3b`, `0x606c8d`), and it went on when the window came
    back.

**The transmuter** (`town/05`; the 360's mode 5, type 134): Duran the Transmuter, a townsman west
of the gem goblins (`MERCHANT_COMBINER`, unit type `COMBINER`). His frame is the merchant's.
- **PC's side.** CGameUI's `+0x2e0` is PC's `CCombineMenu` (made at `0x562ff7`, vtable
  `0xab2c4c`). CGameUI opens it on the interaction 0x11 (`0x557d7d`), with PC's inventory, for
  the unit the hero used (`+0x4c`); its character is at `+0x50` and its open flag at `+0x54`.
  Its four places are slots of the hero's own inventory, numbered at `+0x7c`; the run read 15,
  16, 17 and 18, the slots the 360 looks for, `sub_8236BA20`. Its command 9 transmutes (its
  virtual `+0x30`, `0x56a490` → `0x569550`).
- **The look** (`sub_82367D38`, the catalog):
  - The left column has the plain images, `title.transmuter`, no tabs, and the layout's help
    line `transmutor` with the Y prompt and arrow under the rows. The filter's name is
    `filter.transmute` (the 360's string at `0x820D0C40`).
  - The four places are always drawn; an empty one is the tray at 0.6 with nothing in it.
  - The counter shows the places taken of 4. The bags leave those slots out.
  - The bag is focused at the start, and shows LB and RB and the gold.
- **The buttons and input** (`sub_8236C8D0`, mode 5): X `act.swap` in every column; A and Y have
  no text, and A is not done at the transmuter.
  - X in a bag puts the item into the first free place (PC's slot placing `0x4e9480`, as its
    transmute puts its result back): sound 18, or 24 when all four are taken.
  - X in the transmuter's column takes the item back as from a stash.
  - Y in any column calls PC's transmute.
- **Tested** (GOG, run 5): the frame opened at Duran. X placed the sword in a place (1/4) and
  took it back, and B closed both menus. Y with that one item made nothing (no recipe).
  Run 6: three health potions bought at the merchant (no console) and put in as one stack of 3
  in a single place; Y made a larger health potion (PC's recipe `HEALTHPOTION_UPGRADE1` counts a
  stack), and X took it back. Captures: `screens/gog/transmuter-1`,
  `transmuter-recipe-before`, `transmuter-recipe-after`.
- **The columns' places** (`sub_823788D8`, the merchant's menu, the transmuter's too): -1, 300 and
  603 (constants at `0x820aa334`, `0x820aa5d4`, `0x820aa87c`), not the inventory's 0, 299 and
  602; the review of run 5 found the bags 1.9 pixels left at the transmuter, and the merchant's
  column was 1.4 off.
- **The card's sell price.** The 360's card shows it while its merchant menu (CGameUI's `+920`,
  the transmuter's too) answers its virtual `+0xc`, for an item not of type 103 (`sub_8233FE48`).
  PC's tooltip asks its own merchant menu the same (`0x5471bc` → `0x58dcd0`: open, or its `+0x31`
  clear). The transmuter is another menu in PC, so at the transmuter `+0x31` is cleared for the
  tooltip's call and put back after it, on the error path too.
- **The card's place**: 22 pixels at 1080 into the column right of the focused one (the catalog:
  menus/08 and town/05); it was 11. Open: town/07's merchant card is 25.1 in, not explained.

**The enchanter** (`town/06`; `CEnchantMenuXenon`, vtable `0x8200159c`): Goren, by the stall east
of the brazier (`MERCHANT_ENCHANTER`).
- **PC's side.** CGameUI's `+0x2dc` is PC's `CEnchantMenu` (made at `0x562fb0`, vtable
  `0xab6c0c`). Its owner is at `+0x2c`, the character at `+0x30`, the open flag at `+0x38` and the
  mode at `+0x90`. CGameUI opens it with PC's inventory, in the mode of the interaction: 0x15 is
  the enchanter (`0x557e23`); 0x16, 0x19, 0x1a and 0x1b are its gem, shrine and heirloom pages,
  left to PC for now and logged. Its command 9 (`0x57c900` → `0x57a800`) does the mode's work on
  the item in the hero's slot 14 (`0x57aea4`): the price checked and taken, the enchantment or
  the disenchant, and PC's sounds.
- **The look** (`enchantermenu_xenon`, its 15 windows named by the table at `0x83385300`): the
  panel takes the left column. The bags are at 300 and 603. LB and RB walk the bags only, and the
  bag is focused at the start.
- **The panel** (`sub_82360EC0`, for the bag's selected item):
  - No item: no price, no chance, no prompt.
  - An item that cannot be enchanted: `ench.cannot` (`0x820CF18C`), or `ench.max`
    (`0x820CF128`) at the maximum, written red in the price's window. "The price of the
    enchantment is" stays, and the chance is emptied.
  - Else the price (`CEquipment::enchantPrice` *(OT)*, GOG `0x4b2590`, the 360's `sub_822ABAC8`),
    red when it is over the hero's gold, and `ench.chance` (`0x820CF164`) ": N%". N is the
    settings' base `+0x7c` plus the enchantments (`+0x2f8`) times `+0x98`, at most `+0x80` (GOG
    `0x524d40`, the same offsets as the 360's). The Enchant prompt shows.
  - What can be enchanted (`sub_822B5688`): identified, of type 8, 13, 17 or 24, under the
    maximum (`+0x94`).
  - The 360's red is text markup (`|cFFFF0000`…`|u`), which PC's CEGUI writes as text; here it is
    the window's TextColour instead (first run: "FFFF0000300" on screen).
- **The bar and A**: A `act.enchant` (`0x820CF118`) for an item that can be enchanted. A puts the
  item into slot 14, runs PC's command and puts the result back in its place, an equipped slot
  too.
- **Tested** (GOG, runs 7-9): the frame, the red price (300 over 100 gold) and "Cannot enchant
  item" red for a scroll (`screens/gog/enchanter-1`, `enchanter-cannot-1`). An enchantment
  itself is untested (not enough gold). The card's item name is in the 360's SerifBig now (PC's
  layout had FrizQuadrataBig there), in every screen.
- **The identify scroll's card text** (right click, left click) is the item's own text in PC's
  data, left unchanged.

**Clicks on PC's windows** (runs 8-9). A dialog of PC's (the guide NPC's, by the brazier) took no click on
its Ok; Escape did not close it either. The click log (`x360 click:`, each left click's window
from `System::getWindowContainingMouse`) showed the 360's HUD under the mouse:
`x360hud_SkillSet1`, a 1920x1080 window. Every window of the 360's HUD and pet HUD now lets the
mouse through (MousePassThroughEnabled). So do PC's own bottom HUD and pet HUD: they are kept
transparent under them, and a click on the bar opened PC's skills (its SkillsButton) once the
360's let it through. After that, clicks over the HUD reached the world. **Open:** the dialog's
Ok was not clicked again after the fix (the guide NPC walked off).

**The gem goblins** (`town/02`, `03`): the enchanter's menu in its other modes, in both games.
- PC's CEnchantMenu opens in mode 0x19 for a unit of type 132 and 0x1a for one of type 133
  (`0x499038`, `0x499050`). The 360 titles type 132's page "Destroy Gems" (`title.destroygems`,
  `0x820D1044`; its page `getsocket`, town/02) and type 133's "Recover Gems"
  (`title.recovergems`, `0x820D1028`; `destroysocket`, town/03): its `sub_82360808` sets `+304`
  to 10 and 9, and `sub_82376D48` titles by that. PC's internal names (GEMRECOVER,
  SOCKETABLERECOVERER) read the other way round; the screens follow the 360's titles and pages.
- The page's A prompt shows for an item with gems (the 360's `sub_8236B1C8`; here what PC asks
  before working, the item's `+0x39c` and `+0x3a4`, `0x57c173`). The bag's bar has no button
  (town/02, 03), and no gold: the 360 hides the bag's gold at a menu of mode 7, 8, 9 or 10
  (`sub_82367D38`, `0x82368620`). A runs PC's command on slot 14, as at the enchanter. The result line starts
  empty (the layout has a sample text).
- **Tested** (GOG, run 13): both pages match the catalog window for window
  (`screens/gog/destroy-gems-1`, `recover-gems-1`). Removing gems is untested: the save has no
  socketed item with gems.

**Socketing a gem** (A on a gem in the bag; the 360's `CMessageItemSelectXenon`). **[read; not run]**
- **The 360.** A on a gem (`sub_8236AA48` -> `sub_8236B240`, for a gem in the hero's or the pet's
  bag) opens the global UI's `+6292`, a `CMessageItemSelectXenon` (vtable `0x820D1904`), with the
  gem in its `+44` (`sub_8237ADF0` with 1: the sound 22). It is shown in
  `messagehousing_xenon` (`sub_8237A770`) and holds a `CInventoryMenuXenon` of the hero
  (`sub_8237ACD8`: `+384` = 2, `+388` = 1) whose list (`sub_82369FE8`) takes, with no filter, the
  identified items (ISA 2, ITEM; `+796`) with a free socket (`+916` against `+924`). Its bar has A
  only, `act.insert` (`sub_8236C8D0` when `+384` is not 0; the string `0x820D1070`). Its pad
  (`sub_8237AAF0`, the codes 5 to 24): A keeps the selected item (`+48`) and closes; X and Y go
  to the list's `sub_8236A810`; B and the others close with none (`sub_8237ADF0` with 0: the
  sound 66). The inventory menu then (`sub_8236ECC0` -> `sub_8236EDD8`) takes the gem out of its
  bag (`sub_822E3218`), puts it in the item (`sub_822BAEF0`: the sockets list `+920` grows while
  `+916` is larger), `sub_822B5F78`, recomputes the owner's inventory (`sub_822E0F08`,
  `sub_822E17A8`, `sub_822E0648`) and plays a sound; the list shows the item's category with it
  selected. With no item: the sound 24.
- **PC's own** (a gem dropped on an item, GOG `0x54f474`-`0x54f5d7`): a gem (ISA `0x78`) on an
  identified item (`+0x2fc`) with a free socket (`+0x39c` minus `+0x3a4`): `playDropSound`,
  `0x4b9510` (the item's sockets list `+0x3a0`; the same test), `0x4c01b0`, the UI's sound
  `0x17`, the owner's inventory's `0x4e7c80`, `0x4e9110`, `0x4e75d0` (the owner's `+0x404`,
  `0x54e56d`). The gem was taken out of the bag when it was picked up.
- **Here** (`sock_begin`, `sock_pad`, `sock_finish`): a fourth list (`x360sock_`, the bag's
  layout again) in the message housing's `parent`, drawn by the bag's code with the select's
  rule and A `act.insert`. **Compared with the 360's capture** (the mods agent, 2026-10-22:
  `in-game/10-gems-tab`, `11-gem-socket-select`, `12-gem-socketed`): the select is the bag's
  layout over the bag's column, 18.34 rows (of 720) higher; no tabs; the filter's line reads
  "Insert Into" (`filter.insert`, `0x820D0C54`); "My Bag", the bag's count, no gold; A "Insert";
  the housing's own "Close" (B) under it. All done so. After A the bag shows the item's filter
  (Armor) with it focused, and the gem is gone from the bag (46 to 45), as written. A: the gem taken out of its bag
  (`removeEquipment`), then PC's calls above in its order; the list on the item's category with
  it selected. B, the triggers, the bumpers, Start and Back close with the sound 24. Up and down
  move (the sound 67). X and Y do nothing (the 360's `sub_8236A810` with the select is not read).

**NPC dialogs** (`town/04`, `13`, `14`, `27`-`29`, the quest offer `42`; the 360's
`CQuestDialogMenu`, vtable `0x820010BC`).
- **PC's side.** PC's `CQuestDialogMenu` (vtable `0xab7ff0`) shows a layout whose windows have the
  360's names: Portrait, DialogTextFrame/DialogText, Ok, Accept, Decline, RewardGold/XP/Fame and
  their Box, ItemBkg1-3 and ItemSlot1-3. PC takes its dialog off the GUI sheet to close it,
  which leaves it "visible", so it counts as open only while it is under the sheet (a dump with
  the guide NPC's dialog open:
  `screens/gog/pc-dialog-guide`). The cinema bars (Root, TopBar/BottomBar) are PC's in both games.
- **A tick once a frame.** While PC's dialog is open, PC does not call GetCursorPos unless the
  mouse moves, so the pad's tick (pad.c's `order`, run from that hook) stops. A first try ran it
  from `System::renderGUI`, but CEGUI is drawn only when something changes: once the text had
  risen, the pad went unread until the mouse moved. The tick now comes from an Ogre
  FrameListener of the proxy's, added to PC's Root (`Root::addFrameListener`; its virtuals in the
  order of PC's own listener at GOG `0xabbac4`), once a frame.
- **The screen.** While PC's dialog Frame is shown, the 360's `questdialogmenu.layout` is shown
  over it, every window mirrored from PC's each frame. PC's Frame is kept at alpha 0, and the
  360's HUD is hidden (town/13). "Rewards" with its stick prompt shows when there are reward
  items: town/04 has one, 13 none (inferred, not read in the code).
- **The rising text** (`sub_82357C40`, each frame with the frame's time): t grows to
  T = max(the text's characters × 0.015, 5.0) s (`sub_823568D0`: its `+344` times `0x820aa7d8`, at
  least `0x820aa464`). The text's top sits (1 − t/T) × (its height + 50) pixels below its frame's
  top, at 720 rows (`0x820aa388`). This matches the catalog's tops: 219, 110 and 97.6 in 13, 14
  and 04.
- **The pad.** A presses PC's Accept when shown, else its Ok; B its Decline; both as a left click
  injected at the button's centre. X held makes the text rise faster: `sub_82192640` multiplies
  the frame's time by `[r29+268]` while one of two input actions is held, or a flag of the menu
  (`+452`) is set (bytes 4356/4359 and 4357/4360 of the input state; the branch that sets t to T
  cannot be reached with them at 0 or 1). **Open:** the factor is a value of the running game,
  not measured; `dialog_fast_scroll` in `x360_ui.ini`, 4 by default. Also open: the second
  action's button, and who sets `+452`.
- **The fonts.** The 360's letter spacing and native width (`match_spacing`) were set when the
  inventory first opened; a dialog before that had its text 1.3 times wider (the review). They
  are set when the HUD loads and when a dialog shows now. The dialog's lines then break at the
  catalog's words (`screens/gog/dialog-open` against town/13).
- **The colour markup.** The 360's text marks names with `|cAARRGGBB`…`|u` (a name in gold).
  PC's own dialog draws it (`dialog_pc=1` in `x360_ui.ini` leaves PC's dialog visible: a
  diagnosis). With the same text, the window drawn in PC's SerifBig (`dialog_font=SerifBig`)
  showed the name in gold, and in the converted X360_SerifBig the markup came out as text. But
  PC's font breaks the lines elsewhere than the 360 (its letters narrower), so the 360's font is
  kept and the markup taken out (`dialog_markup=1` keeps it; since 2026-10-23 it is kept by default, `dialog_markup=0` takes it out: the installer's ini has no such key, and with the old default the names' gold, seen in QA run B, was lost). **Open, high priority:** the names'
  gold with the 360's font. **Stopped after two tries:** (1) PC's SerifBig draws the gold but
  breaks the lines elsewhere; (2) PC sets `Font::setColourTrigger("|c")` and
  `setColourUnTrigger("|u")` on its fonts (GOG `0x55fcf0`, `0x55fd1d`, this build's own exports);
  the same calls on the X360_ fonts (`set_colour_triggers`, run 39: called for each, logged)
  changed nothing, the markup still drawn as text. Not known yet what else PC's fonts have.
- **Open:** the reward item's icon (the slot shows empty; PC draws its icons in windows of its
  own, `QuestRewardSockets`).
- **Tested** (GOG, runs 15-19; the user opened the dialogs with the mouse):
  - the guide NPC's dialog (Ok only) and the quest NPC's quest offer (Accept, Decline, the rewards and an item).
  - The text rose and stopped at its frame's top, and the fonts match the 360's.
  - A pressed PC's Ok and closed the dialog; B pressed PC's Decline and closed the offer.
  - X held was read; its speed was not measured (the captures came after the text had risen).
  - Captures: `screens/gog/dialog-open`, `quest-offer-1`.

**Object labels** (`in-game/04`-`08`). The engine labels the objects near (chest, shrine, lever,
fishing hole, an item on the ground) with a window each under the Ingame UI Sheet, placed over
the object. PC's are of type GuiLook/ItemText (created at GOG `0x55aa5e`); the 360's look is
ceguiWidget/ItemTextButton, which draws the `a_button` 20 pixels left of the text in the Enabled
state only. The 360 enables the label of what A would use, in white (`FFFFFFFF`), and disables
the others, in grey (`FFCCCCCC`): the catalog's 07, "A Chest" with "healing shrine" grey
behind. A gdb run on the recomp showed that `TargetInteractHover` of hud.layout is never written
and is hidden in every catalog capture.
- Each of PC's labels is given the 360's look once (`setLookNFeel`; checked by its `LookNFeel`
  property) and the 360's Serif. The label whose text is the name of pad.c's candidate (`+0x420`,
  or the item's name) is enabled; the others are disabled. The state is written only when it
  changes.
- **The A button's size and place.** In the 360's look the button is 21×21 pixels at −20, −1, in
  the 360's 720 rows, and PC's looks do not scale with the screen. The converter writes it as
  fractions of the label's height (`label_button_relative`), and the label is given the 360's
  height (17.27 of 720), its centre kept: PC's engine makes it 40 high at 1080.
- **Two of the same name** (three "Chest" were lit in one dump): only the one nearest the
  screen's centre, where the hero is, is lit. **This is not the 360's rule**, an approximation.
  PC keeps no link from the label to its unit (a pool of windows). Open: project the
  candidate's position instead. Read 2026-10-22: PC places a label (GOG `0x55a9c6`-`0x55a9e9`)
  with the unit's `getPosition` (`0x510eb0`) and CGameUI's `0x541800` (thiscall; the output, the
  position, a view-projection Matrix4 by value built just before from the camera, a float;
  `ret 0x4c`), which scales by the screen's size at CGameUI's `+0x13e0`-`+0x13e8`. Doing the
  same needs that matrix built as PC builds it (`0x55a999`: 16 words from `ebp-0x37c`); not
  written without a run to check it.
- **Open:** the 360's label background is darker (its StaticShared frame, also in 720-row
  pixels).
- **Tested** (GOG, run 40; the user walked the hero): "A staff" on an item on the ground and
  "A Chest" (`screens/gog/label-item-1.png`, `label-chest-1`).

**Fishing** (`in-game/09`). PC's `CFishingMenu` (CGameUI's `+0x31c`, vtable `0xab72a0`; open
`+0x18`) and the 360's `fishing.layout` size their ring by the same rule. The 360's is
`sub_821FFE90`, read on the recomp with gdb watchpoints on the ring's area; PC's is at `0x57c997`.
The player is `[[menu+0x1c]+0x1c]`, and f = sin(phase `+0x7a0`) × 0.2 + base `+0x79c`, or 1.0 when
the fish bites (`+0x7ac` > 0). The ring's lower right corner sits at f × 128 pixels of 720 rows;
its upper left is kept.
- While PC's menu is open the 360's layout is shown, and PC's ring, target, button and base are
  made transparent.
- A gives PC's menu its command 0xe and B its command 6 (its virtual `+0x14`, `0x57d060`).
  Command 0xe is the catch: run 41, A shown as PC's "You caught nothing!" when pressed before a
  bite.
- **Open:**
  - PC's ripples: the 360 has none (its layout has no such window and its code names only
    `FishingRing`); made transparent each frame (written 2026-10-21, not run yet).
  - The result ("You caught …") is PC's window.
  - B (cancel) is untested.
- **Tested:** `screens/gog/fishing-1`, `fishing-catch.png`; the label "A helm" with the
  360's button size (`label-item-2.png`).

**Target bars** (`in-game/01`-`03`). A dump of the enemy's bar (`screens/gog/target-enemy.json`,
taken by the proxy at the first live enemy shown: `dump_targets=1` in `x360_ui.ini`) against
in-game/01 differs only in the unit's data: name, level, description, and the fill (full
health). **Open:**
- The champion (02): none met in the save.
- The pet or an NPC (03, the pet): PC's mouse-over does not give the pet, so the bar does not
  show it. Not known what makes the 360 show it.
- A unit killed stays in the bar, empty, for the rest of the 3 s the attacked one is shown (run
  41: a monster's, empty). The 360's rule for a dead target is not read (`sub_821A83C8` is long).

## 15. The final list (2026-10-17)

Every screen of the catalog in scope is ported and compared against the 360's; what follows is
what they still lack. The screens done are in section 14.

**The user's decisions (left PC's on purpose):**
- The front end: the title, main menu, options, new and load character.
- Exit to Title, and the pause's Options, Achievements and Leaderboards.

**Work not done or not finished, most visible first:**
1. **The dialogs' names in gold.** PC's own fonts draw the `|cAARRGGBB` markup and the converted
   X360_ fonts do not. Two tries failed: PC's font breaks the lines elsewhere, and the colour
   triggers did not take. The names show white. A diagnosis (`font_diff=1`, runs 47-48)
   compared PC's SerifBig object with X360_SerifBig in memory. The vtable is the same (one
   class). After the triggers, the object's `+0x2f0`/`+0x2f4` (a vector, PC's of 2 entries,
   X360_'s 0 before) are equal. The words that still differ are pointers and padding bytes, and
   PC's object reads as text past `+0x8` (another layout, or other memory). Nothing found in the
   font object explains the gold. Next steps:
   - Confirm that the pointer read as PC's SerifBig was the Font: getFont's result and
     FreeTypeFont's vtable. If it was not, the diff compared different things, and `+0x2f0` is not
     conclusive.
   - Create the X360_ fonts with the function PC loads its own with (near GOG `0x55fc60`, the one
     that sets the triggers) instead of the scheme, then apply `match_spacing`. Whatever else that
     path does is then inherited.
   - **Read 2026-10-21:** that path (GOG `0x55fc5e`-`0x55fd26`) sets `Serif` on a window, gets
     the font, sets the triggers `|c` and `|u`, and then **sets the font's byte `+0x424` to 1**.
     The diff read `+0x200..+0x400` only, so it missed it. Written: `set_colour_triggers` now
     sets `+0x424` as PC does, and the diff reads to `+0x440`. **Run B (2026-10-22): done**,
     the quest NPC's target name shows in gold.
2. **The quest reward item's icon** in the dialog. Written, not run yet: PC draws the icon in a
   window under its `ItemSlotN` (`icon__N`; the dump of the quest NPC's offer), so the first image under
   PC's slot is copied to the 360's. Its sockets come from PC's overlay over that slot
   (`QuestRewardSocketsO`: "onesocketglow", "twosocketglow"), shown with the 360's
   `EmptySocketN_0`/`_1`.
3. **The object label of the candidate.** It was matched by text and, of several of one name, by
   the screen's centre. **2026-10-22:** PC's label loop (GOG `0x55a8db`-`0x55b248`, over the
   level's units) keeps each unit's label window at the unit's `+0x198` (made when 0 at
   `0x55a9ef`, placed with setPosition at `0x55b21b` after the projection `0x541800`), so the
   label enabled is now the candidate's own window; the text match is kept only for a candidate
   without one. No projection needed. Not run yet.
4. **The object label's background**, darker in the 360 (its frame's pixels are 720-row ones).
5. **Target bars:** the champion's and the pet's or NPC's are not compared. The rule for a dead
   target, read 2026-10-21 (`sub_821A83C8`, about `0x821A9200`): the unit is the one under the
   cursor if it is of type 104, else the player's virtual `+460` (the one attacked); the bar is
   shown if it is of type 167 and its state is not `MIMICIDLE` (`sub_822801D0`). There is no
   test of death there, so a dead unit keeps its bar, empty, while it is the target: as the port
   does (the attacked unit for 3 s). The types are the same in both games: `UNITTYPES.HIE` has
   the same 162 IDs in both paks (read 2026-10-22: 2 ITEM, 104 NPC, 167 MIMIC).
6. **The merchant's screen sides** keep PC's panels (PC narrows the world's viewport; stopped
   after two tries), and PC's first-time Tips show after closing a shop.
7. **Fishing:** PC's result window (the ripples made transparent, not run yet). A (catch) is tested:
   PC's catch command, `fishing-catch.png`. B (cancel) is untested. A catch timed to a bite is for
   the user to play: it takes reflexes.
8. **Smaller:**
   - the quest NPC's "?" seen through the dialog's panel.
   - The Fast Scroll factor: 10, the 360's (`[r29+268]` in `sub_82192640`, r29 the constant pool
     `0x820AA2C0`, its `+0x10C` 10.0; the mods agent's static reading, 2026-10-23).
   - The gold hidden at the gem pages is not seen in a run.
   - The identify scroll's card text is PC's mouse text (the item's data, unchanged).
   - The card's place at the merchant: 25.1 pixels into the middle column (the bag's), against 22
     into the third, in every one of the catalog's merchant captures (town/01, 07, 09-11, 18,
     36-38) and the inventory's and transmuter's (menus/08, in-game/10, 18, town/05). Ported by
     column (2026-10-23), not run.

**Untested for want of the means in the save:**
- An enchantment: done in run B.
- Removing gems (no socketed item; socketing with the pad is written since, not run).
- The Gambler's purchase: done in run B. Selling from the pet's bag: not tried.

**Earlier lists** (sections 11-13) still open:
- The converter must decrypt the player's XEX itself.
- The Skills page's requirement line; its X (the mapping) is written, not run (section 14).
  **2026-10-22:** the description is now built as the 360's `sub_823B07C8`: the cooldown line
  (`sub_823C0A60`, GOG `0x5df280`: the rank's `+0x70` for a player, else `+0x6c`), the mana line
  (the rank's `+0x4c`, after the rank's own text, the 360's `+288`, GOG's `+0xe8` as PC's tooltip
  `0x5d8d80` reads it), the weapon requirement (`sub_823B0230`: the first rank's
  REQUIREMENT_RIGHT `+8`, REQUIREMENT_LEFT `+0xc`, DUELWIELD_REQUIRED `+0x10`, GOG's loader near
  `0x5dfb72`; the type's name by `sub_823AFDB0`: melee 60 or ranged 35/163, single 9 or two-handed
  10, else any, tested with GOG `0x5fe190`), then the effects (GOG `0x5d8c60`), each line in
  `|cFF9180FF` .. `|u`. The 360's capture `test/04` ("Right hand requires : melee Weapon",
  then the effects) is the check. The texts are the 360's (`skill.*`, `req.*`, `wt.*`). Not run
  yet.
- The target bar's size, checked here only for the enemy.

## 16. Steam: the port, done offline (2026-10-23), and the test still to run

**State:** phase A below is done, offline, at the coordinator's request (2026-10-23; the plan of
2026-10-21 had put it on hold until the GOG port worked). Branch `steam-port`. Nothing was run:
no game, no Proton, no wine. Steam's install was only read (`Torchlight.exe`, `CEGUIBase.dll`
and the DLLs the proxy names, in `steamapps/common/Torchlight`); `tools/steam_guard.py verify`
against the last snapshot (`steam-guard-qa21.json`) printed UNCHANGED before the work. Phase B,
the one run, still waits for the user's OK. README's Steam badge says "experimental" (ported, not yet played).

**Why a port is needed:** the proxy names 305 addresses of GOG's `Torchlight.exe` in
`proxy/*.c`: 164 in its code (functions it calls, patch sites, vtables, IAT slots, strings and
floats in `.rdata`, globals in `.data`), 141 in comments only (the GOG sites the comments cite).
Steam's executable is build 17685 (timestamp `0x4b5f7519`), GOG's 17870 (`0x4ffc6815`), section 5:
none of those addresses holds there as it is.

### 16a. What differs between the two executables (read)

- **Code generation.** GOG's build has stack cookies (`/GS`: `mov eax,[cookie]; xor eax,esp` in
  every function with a frame) and sets up its exception frames as `push -1; push handler;
  mov eax,fs:[0]`; Steam's has no cookies and starts `mov eax,fs:[0]; push -1; push handler`. So
  the first bytes of most functions with a frame differ, and their stack offsets (`[esp+0x24]`)
  too. The rest of the code is the same, but for register allocation here and there.
- **Steam's executable can be relocated:** its DllCharacteristics are `0x8140` (DYNAMIC_BASE) and
  it has a `.reloc` section; GOG's has neither. Both are linked for `0x400000`. The proxy's
  addresses assume that base (see 16d).
- **Struct layouts: the same** wherever compared (16c).

### 16b. The address map: `proxy/addresses_steam.txt`

`tools/port_addresses.py GOG_EXE STEAM_EXE --source proxy` (now with `--code-only` and
`--table`) looks for each address in independent ways, and writes down the ones that agree:

| Way | What |
|-----|------|
| window | GOG's bytes at the address, call displacements and in-image values masked, grown until one match is left in Steam's code; values pointing into `.rdata` compared by what they point at when several matches remain |
| body | windows from inside the function, each match's function start (the function starts: 16-aligned call targets, vtable entries, code after int3 padding); two must agree |
| callers | each GOG call site of the function matched inside its own function's copy, Steam's call target read |
| vtable | the function's slot in a GOG vtable, the same slot of the Steam vtable with the same RTTI class |
| local | an address inside a function: the window looked for only inside the function's copy |
| near | code 48 to 256 bytes before and after matched alone, the address's window looked for only where each anchor puts it |
| strings | the strings a function names, unique in both builds: the functions naming each, in address order, are as many in each build |
| refs | for data: each code site holding the address matched (inside its function's copy), the 4 bytes there read |
| rtti, import, string | a vtable by its class's RTTI name and locator offsets; an IAT slot by its DLL and function name; a string by its whole content, unique |

The window, body and local ways may mask the stack displacements (`[esp+d]`, `[ebp+d]`) when they
search inside a small region. A call site or reference matched in the whole code instead of its
function's copy is weak: it never overrules the others. **verified:** two independent ways agree
and none disagrees (or three and more outvote one call site matched elsewhere, a small caller
like many). **single:** one way only. **unresolved:** none, or the ways disagree.

The table (numbers and the ways' names only, no bytes of the game) has every one of the 305
addresses:

| Where the proxy names it | verified | checked by hand | single | unresolved |
|---|---|---|---|---|
| code (164, used by the build) | 160 | 4 | 0 | 0 |
| comments only (141) | 112 | 0 | 19 | 10 |

The four checked by hand, each with its evidence in the table:
- `0x56cd00` (CConsole::executeCommandLine *(OT)*) -> `0x567f60`: its body after the prologue,
  stack offsets masked, is once in each build; 29 direct call sites in each.
- `0x591200` (the options menu's open) -> `0x58bd20`: COptionsMenu's vtable slot `+0x18`, the
  strings it names (7th of 12 functions in both), the same instructions but the prologue, and its
  callees and globals map as the table says (`0xf60d9c` -> `0xf36e34`, `0x5366f0` -> `0x532710`).
  The body windows had chosen `0x5788b0`, a copy of the same code for CInventoryMenu: the body
  way alone is not trusted.
- `0x5d68e0` (a skill's kind text) -> `0x5d0580`: its one call site in each build, the same
  instructions but the prologue, its neighbours `0x5d5c00` and `0x5d7d40` moved by the same amount.
- `0x70ced2` (the modal function's exception handler, which the hook checks) -> `0x707622`: the
  handler pushed by the verified modal function `0x5482f0`; a frame handler stub as GOG's is.

Sanity checks on the result: every function pair's instructions compared over the whole function
(addresses and stack offsets normalized): the least alike are the ones whose prologue and
register use differ, none with other code. The comments' unresolved addresses (no Steam address in
the table): `0x546c9f`, `0x549b70` (the ways disagree), `0x54f01f`, `0x5588e8`, `0x55b21b`,
`0x55fcf0`, `0x562fb0`, `0x574477`, `0x57de89`, `0xab72b8`; and 19 found one way only (in the
table as single). The proxy's code uses none of them.

### 16c. Struct offsets

- `tools/compare_offsets.py GOG_EXE STEAM_EXE proxy/addresses_steam.txt` disassembles both copies
  of each function in the table (GNU objdump, up to the first int3 or the next call target) and
  aligns their object field displacements (`[esi+0x304]`; not the stack's). Of 162 function pairs,
  **148 have the same list**. The 14 others were read: a jump table decoded as code, an extra load
  from register allocation, or code one build inlines and the other calls (the merchant's and
  stash's updates, the combine menu's, the enchant act `0x57a800`); no field at another offset
  (e.g. the transmute, `0x569550`, keeps `+0x1e4`, `+0x20`, `+0x8` and differs only in which
  register holds the object). One constant differs: in `0x5e0fc0` (the skill text writer the
  proxy's comments cite; the proxy calls `0x5d8c60`) `lea ebp,[edi+0x22]` with `edi` 0 in GOG is
  `0x34` in Steam, a number, not a field (a text or index the later build changed).
- Of the 97 offsets the proxy adds to an object, 86 appear at the same place in a pair. The 11
  others were checked at the GOG site the proxy's comments cite and its Steam copy: the font's
  `+0x424` (`0x55f5b0` -> `0x55a950`, 4 writes each), CEquipment's icon `+0x24c` (createIcon,
  `0x4b9b70` -> `0x4b75b0`), CGameUI's `+0x1074` (the shop's click, 3 reads each), `+0x13d0` and
  the skill's `+0x43`, `+0x180`, `+0x184` (`0x5404c0` -> `0x53bf40`), `+0x13ec` (the console, stored
  after the same call at `0x5636a9` -> `0x55ea43`), the fishing player's `+0x79c`, `+0x7a0`,
  `+0x7ac` (`0x57c950` -> `0x577830`): all the same. The hero's `+0x224` is only logged.
- CGameUI's menus `+0x2cc..+0x31c`, `+0x304`, `+0x318`, `+0x16b8`, `+0x173d`, `+0x13f8`,
  `+0x106c` are in pairs with the same lists (CGameUI's update `0x55d0c0` -> `0x558510`, 62
  operands; the modal; the side-panel tests; the cursor function; the label loop `0x5578e0`, 589
  operands, one extra load).

**The patch sites, read in Steam's build** (all as the proxy expects):
- the six hotkey jumps (`0x55d206` -> `0x55863a`, `0x55d282` -> `0x5586b6`, `0x55d2a4` ->
  `0x5586d8`, `0x55d2c5` -> `0x5586f9`, `0x55d2e7` -> `0x55871b`, `0x55d318` -> `0x55874c`): the
  same two bytes, and the code up to each new jump's target the same, so `eb 16` / `eb 07` land on
  the next test as in GOG;
- the side-panel tests (`0x541090` -> `0x53cb10`, `0x541130` -> `0x53cbb0`): `push ebx; push esi;
  mov`;
- the cursor function (`0x547ca0` -> `0x543660`): its first 7 bytes and the rest the same;
- the modal (`0x54cad0` -> `0x5482f0`): `mov eax,fs:[0]; push -1; push 0x707622`, `ret 0x3c` as
  GOG's;
- the bag (`0x49f85f` -> `0x49d6c3`, `0x49f880` -> `0x49d6e4`, `0x49f88f` -> `0x49d6f3`): `push 21`,
  both calls to addSection (`0x4e9a00` -> `0x4e7370`), the same code between but stack offsets.

### 16d. The build, and what the proxy checks at load

- `tools/build_proxy.py build REAL_DLL OUT --target steam` copies `proxy/*.c` to `OUT/src` with
  every address in their code (outside comments and literals) replaced from the table's
  verified and checked lines, and compiles them with `X360_TARGET_STEAM` defined; an address
  without one stops the build. `--target gog` (the default) builds `proxy/*.c` as written. The
  build refuses a REAL_DLL whose folder holds a `Torchlight.exe` of another build than the
  target. Built here (2026-10-23), both variants, into `~/torchlight-gog/steam-port-build/{gog,steam}`,
  Steam's from Steam's own `CEGUIBase.dll` (read): 3839 exports, the 10 hooked ones present; every
  name the proxy resolves in CEGUIBase, OgreMain and msvcp90 is exported by Steam's DLLs too.
- `X360_TARGET_STEAM` changes only the target build's timestamp and the modal hook's prologue
  (Steam's: `mov eax,fs:[0]; push -1`, 8 bytes moved to the copy; GOG's: `push -1; push handler`,
  7). The bag patch now checks that its two calls go to addSection (the mapped address) instead
  of their displacement bytes.
- At load the proxy logs the build it was made for, and if `Torchlight.exe`'s PE timestamp is not
  that build's, or the executable is not at `0x400000`, it logs both and leaves the pad, the
  360's screens and the bag patch off: only the converted scheme and layouts load (through CEGUI
  and Ogre by name). Every byte check at the patch sites stays.

### 16e. Phase B, one run (the user's approval, and the agents told before and after)

Section 4's precautions hold, and `tools/steam_guard.py snapshot` before, `verify` after
(UNCHANGED, or stop and tell the user).
1. A prefix of its own (`~/torchlight-steam/compat`), never `compatdata/41500`. The game folder
   is copied into `~/torchlight-steam/game/`, so the proxy never lands in Steam's install. The
   test save comes from GOG's prefix (the mine save), not from the user's Steam saves.
2. Steam closed (checked with `pgrep`, not assumed), Steam Cloud off for the game, and a
   `steam_appid.txt` with 41500 next to the copied executable, so Steamworks does not restart the
   game through Steam (`SteamAPI_RestartApp`, which would open it in the real prefix). The run
   script refuses `compatdata/41500` as `run_gog.sh` does, and runs without `steam` on the PATH.
3. The proxy built with `--target steam` from the copy's `CEGUIBase.dll`. First thing to read in
   `x360_ui.log`: "built for Steam's build 17685" and no "is not Steam's build" line (the base:
   Wine normally loads an executable at its preferred base even with DYNAMIC_BASE; **not seen
   yet**). Then that the patches logged themselves (cursor hooked, modal hooked, hotkeys off 6 of
   6, the bag if `bag=360`).
4. If the game exits at once, or Steam starts: stop, `verify`, tell the user. No second try
   without a new plan.
5. The checks, short: the HUD, the inventory and Skills pages, a shop, an NPC dialog, a label,
   pad movement. Then quit, `verify` (UNCHANGED), and say "done".

**What may go wrong, known now:** Steam's executable needs `steam_api.dll` and may refuse to
start without a running Steam. If `steam_appid.txt` does not make it start, the Steam test stops
there and the user decides. Starting Steam would mean the real client near the user's prefix, and
that is outside this plan. What the offline work cannot show: behaviour that differs between the
builds without changing the code's shape (data, timing), and the executable's load address.

## 17. QA of the work written without the game (2026-10-21)

Everything below was written with the game closed and is not run yet. One session on GOG checks
it: the proxy built at `43efb49` or later installed, `steam_guard.py snapshot` before and `verify`
after, and `dialog_markup=1` added to `x360_ui.ini` for step 4 (`font_diff=1` stays: its log
line now reads to `+0x440`). Each step names what to look at; the log is `x360_ui.log`.

1. **Skills mapping:** open the inventory, RT to Skills, select a learned active skill, X: the
   overlay `skillsmap_xenon` shows (its place against the page is not known). Release B: the
   overlay closes, the sound plays, B's icon on the page's set and the HUD is the skill. Then
   close and press B in play: the skill fires. Repeat for Y, LT, RT; X on a passive or unlearned
   skill does nothing; X again, Start or Back close the overlay.
2. **Tip:** a first-time tip (a new character's first town, or the first closing of a shop or the
   stash): the 360's tip over PC's, with PC's title and text; B closes it; Y on another turns the
   tips off (none shows again).
3. **Fishing:** the ripples gone under the 360's ring; A before a bite: the result is the 360's
   modal ("Fishing", the text, A Ok); A closes it, and does not start another cast.
4. **The names' gold:** an NPC dialog whose text has a name (the guide NPC, the quest NPC): the name in gold
   (`dialog_markup=1`). If it is not, the log's `font diff` lines at `+0x424` say whether the
   byte took.
5. **Regressions:** the NPC dialog's A/B still press Accept/Ok/Decline (its press code was made
   common with the tip and the modal); the inventory's B, LT/RT still close and change pages
   when the mapping is not open.

**The QA in three runs (the mods agent's recipe, 2026-10-21).** The console commands named are in
GOG's executable's command table (LEVELUP, SKILLPOINTS, MONEY, ITEM, CHAMPION, UNIT, KILLALL,
AIFREEZE); `GOD`, `CHAMPION SKELETON` and `UNIT FISHING HOLE` were seen working on the 360's
console (the recomp), not yet on GOG. The proxy runs `console=` at the first opening of the
inventory (section 13), so each run's batch is one.
- **A. Tips, a new hero, no console** (the seen tips are kept per character, inferred): create a
  hero and enter the town: TIP_WELCOME; the first attack, the first inventory, stats or stash give
  more. B on one, Y on another (step 2). Also step 5's regressions.
- **B. Town, on a copy of `variety`** (already marked by the console): `console=LEVELUP;LEVELUP;
  LEVELUP;SKILLPOINTS 5;MONEY 20000;ITEM <the gem's name>,2` (the gem's shown name; if the
  console wants the unit's `NAME`, it is in `socketables/GEM_AMETHYST_01`). Learn an active and
  a passive skill by hand, map them (step 1); socket a gem in the unique sword (a unique: the
  unique bases carry `SOCKETS`) and take it out at the Gem Recoverer and the Destroyer;
  enchant; buy from the Gambler; talk to the guide NPC and the quest robot NPC (step 4, `dialog_markup=1`).
- **C. Mine, on `mine-level1`:** `console=GOD;KILLALL;CHAMPION SKELETON;AIFREEZE;UNIT FISHING
  HOLE`: the champion's gold target bar, then kill it and fish (step 3).
- **B2. Town again, on the save as run B left it** (no console; the two amethysts and the unique sword are in the bag): A on a gem, the item select, A on the sword; then the gem pages
  (Recover, Destroy). Map a skill and see the page's set change at once; enchant and see PC's
  inventory close; the first talk to an NPC (no flash of PC's dialog); the log's timings of the
  first opening. In the inventory and at a merchant: LB/RB change the column, the left stick's
  left and right the filter (changed 2026-10-22). Can follow C in the same session.

**Run A (2026-10-21, the user with the 8BitDo; a new hero, slot 1).** Steam's folders UNCHANGED.
- The tip shows as the 360's. B closed it **and** attacked (`pad.c` sent the hotbar key 3 too):
  fixed in `410b9f2` (no world action while a 360 tip, modal or dialog has the pad, nor 0.3 s
  after it closes), not run yet.
- Y: the log shows PC's `ShowTips` pressed, then `Ok`, and `settings.txt` was saved with
  `SHOW TIPS :0`: PC's own option went off. A tip still showed after it. PC queues its tips
  (`CGameUI` `0x55f570`: shown if the hero has not seen it, `+0x9c7` + the tip's index, and the
  setting `[0xf60d2c]` is 1, then pushed to `+0x16dc`). Opening the stash opens PC's inventory,
  so TIP_INVENTORY and TIP_STASH are queued together and show one after the other on closing:
  the second was already queued. That is PC's queue. Whether the 360's Y also drops a queued tip
  was not read.
- The tip's text spoke of the keyboard (PC's). The 360's GLOBALS has the same tips
  (CONTEXTTIPS, 24 in each game, 23 in both) written for the pad: the converter's `--pc-pak`
  writes `tip.ID` and `pctip.ID` into `x360_strings.txt`, and the proxy shows the 360's text of
  the tip whose PC text PC set (TIP_HOWTOATTACK, PC's only, stays PC's). Not run yet.
- TIP_INVENTORY did not show at the 360 inventory's first opening: PC requests it (one of the 24
  callers of `0x55f570`, by index) when PC's own inventory opens, which the 360's does not open.
  It showed with the stash. Whether the 360 requests it from its inventory was not read.
- An NPC with a quest showed PC's dialog, not the 360's, and the pad kept attacking. The log has
  no `x360 dialog` line: the 360's dialog never loaded. Not explained yet; a UI dump 1.5 s after
  each A on a unit (`talk-N`) was added to see which of PC's menus opened.
- The inventory: B closes, LT/RT change pages. The font's `+0x424` byte now reads 1 in the
  X360_ fonts too (the font diff); the names' gold is for run B.


**Run B (2026-10-22, town, the variety copy with the console).** Steam's folders UNCHANGED.
- Dialogs: the 360's over PC's for every NPC (quest and plain); the names' gold shows (a quest
  NPC's target name, `+0x424` read). An NPC whose text has no colour markup shows it white, as
  written. The first talk showed PC's dialog for a moment: PC's was looked for every 250 ms; it
  is now looked for at every frame for 10 s after the pad's A on a unit
  (`x360_dialog_expect`). Not run yet.
- Tips: the 360's texts (TIP_LEVELUP, TIP_MANALOW, TIP_SOCKETABLE, TIP_ENCHANT); B closes them
  without attacking.
- Skills mapping: assigning works; the same skill can sit on two buttons, and there is no way to
  clear a button, as the 360 (`sub_82382720` writes the slot only; no clearing path). The page's
  set at the bottom kept the old icon until the cursor moved: PC marks its hotbar changed after
  placing a skill (GOG `0x540623`, CGameUI `+0x13d0` = 1), which the port did not; now set, and
  the page filled again 100 ms later, as the 360 redraws the page at once (its `+1920`'s slot
  8). Not run yet.
- Enchanting: done; PC closed its enchant menu but left its own inventory open (its doll
  rendered there, the 360's doll empty): PC's inventory is now closed when PC closes the enchant
  menu by itself. Not run yet.
- The Gambler: the 360's page, X buys (the item revealed on buying). PC's merchant and
  inventory frames still show beside the 360's frame (known, the shop's sides).
- Gems: socketing with the pad is not ported (A on a gem shows `act.socket` and does nothing);
  PC's inventory did not take a mouse click either, so the gem goblins' removing is still
  untested.
- The first opening of the 360's inventory was slow: it runs the console (`console=`) and loads
  the frame's layouts; both are now timed in the log.
- No sound at all: the prefix's audio drivers fail (`winepulse.drv`/`winealsa.drv`: "Unable to
  load UNIX functions"); this machine has no 32-bit libpulse/libasound, and `run_gog.sh` runs
  Proton outside the Steam Linux Runtime. Not the proxy.

**After run B (offline, 2026-10-22).**
- TIP_INVENTORY: the 360's inventory asks for it when it opens (`CInventoryMenuXenon`'s virtual
  `+28`, `sub_8236F700` -> `sub_82337A10(0)`). `sub_82337A10` is the 360's tip request (the
  hero's seen byte `+2471` plus the index, the setting at 1, pushed to the UI's `+6072`), PC's
  `0x55f570`. Index 0 is TIP_INVENTORY in both games' CONTEXTTIPS (the 360's list has
  TIP_SKILLS at 8 and no TIP_HOWTOATTACK, so other indices differ). The port now asks for it on
  opening, but at a shop, where PC's own inventory asks. Not run yet.
- Y on a tip and the queue: the 360 pops its queue (`sub_821A83C8`, the UI's `+6072`) without
  reading the setting again, so a tip queued before Y still shows, as in PC. Run A's tip after Y
  is the 360's behaviour too.
- Socketing a gem with the pad: section 14 (the gem goblins). Not run yet.

**Run B2 (2026-10-22, town, the save as run B left it, no console).** Steam's folders UNCHANGED.
- Gems, as the 360's captures (in-game/10-12, town/02-03): A on a gem opened the item select
  (Insert Into, A Insert, no gold, over the bag) listing the unique sword; A socketed it
  (Sockets 1/1, its effect added, the gem gone, Weapons selected); B closed the select with none.
  The Destroyer kept the sword and took the gem (Sockets 0/1); the Recoverer destroyed the sword
  and gave the gem back; both results showed in the 360's modal.
- The inventory's input: LB/RB change the column, the left stick's left and right the filter.
- The skill set at the page's bottom changed at once; the first talk showed no PC dialog; pet
  commands (Start) and the pet to town worked; the enchanter itself was left for a later run.
- The first opening: the frame's layouts took 394 ms (the console under 50 ms).
- Differences found, fixed after the run (not run yet):
  - A on a gem read Equip: the 360's bar shows Socket (in-game/10);
  - the housing's Close sat at the screen's corner: placed under the select as in-game/11
    (closebutton at 548.13, 649.81 of 720, 50.16 right of the select's TopFrame and 585.59 below
    its top);
  - the rows' socket marks: the 360 (`sub_82368980`, `0x823697F8`) shows the row's
    `socket0_`/`socket1_`, the filled ones first (`socket_full`), then the empty
    (`socket_empty`), from the item's sockets (+916) and filled (+924); PC's `+0x39c` and
    `+0x3a4` (`0x4b9510`), read for weapons, armour and trinkets;
  - A held from the talk accepted the quest at once: the dialog now ignores a button held from
    before it showed, as the tip and the modal already did;
  - the frame's and the pages' layouts are loaded when the game is entered (the 360's HUD up),
    not at the first Back (the user's request).
- The mods agent's 360 captures after the run (in-game/13-18):
  - The gem goblins have no modal on the 360: the result is written in their page, yellow, in
    `getsocketresult` (357,700 to 721,924 at 1080) under the help text, and the page stays open;
    the next A clears it (in-game/14). PC shows it in its modal (`0x54cad0`, from the act's gem
    branch, `0x57c3cc`-`0x57c411`), which closes PC's menus. The port now takes the modal's text,
    presses its Ok and opens PC's enchant menu again with the goblin's type (CEnchantMenu
    `0x575990`, thiscall (open, type): its `setOpen` `0x576540` passes 0x15; the type is its
    `+0x90`). Not run yet.
  - The pet's return is a modal on the 360 too (in-game/16): title "Pet", the text, A OK, over a
    translucent dark rectangle of `FrameB`'s size (585,177 to 1335,764), so run B2's dark
    rectangle is the 360's. No parchment on the 360: the parchment with chains in run B2's photos
    stood at the gem goblins' stall in the world (its chains show above the shop's frame too), to
    be confirmed by the UI dump now taken 0.5 s after each modal shows (`modal-N`). The 360 also
    shows black bars top and bottom (0-108, 972-1080) while it is up: not ported.
  - The card: at the selected row's top, pushed up from row 5 on so that it ends 36 rows (of 720)
    above the screen's bottom (1026 of 1080), past the frame's bottom (in-game/17, 18). Ported.
- The bars at the screen's top and bottom (in-game/16, town/04): the 360's `CInteractiveMenu`
  (its layout `media/UI/interactivemenu.layout`, a `.layout` in the 360's pak: `TopBar` and
  `BottomBar`, 10 % each, `set:Loading image:background`, AlwaysOnTop; the mods agent's reading).
  Built by `sub_82352E30` (from `sub_823392E8`, kept at the UI's `+992`). `sub_82343DF8` shows
  it when the UI's `+5612` is 1 and hides it at 0, hiding the HUD (`+60`) meanwhile; `+5612`
  counts the open menus whose `setOpen` (the virtual `+0x1c`) adds or takes one: CModalMenu
  (`sub_82353528`), CQuestDialogMenu (`sub_82355668`), CDieMenu (`sub_8235F2E8`),
  CSettingsMenuXenon (`sub_823807E8`), CLeaderboardsMenuXenon (`sub_82372480`) and one more
  (`sub_82304928`). The converter now takes that layout (`EXTRA_LAYOUTS`), and the proxy shows the
  bars while the 360's modal or NPC dialog shows, with the 360's HUD hidden. Not run yet.

**Run B3 (2026-10-22, town and the mine, no console).** Steam's folders UNCHANGED. Sound works
(the user installed the 32-bit sound libraries).
- Confirmed as the 360's captures:
  - the frame and pages preloaded (the first opening at once);
  - Socket on a gem's bar; the select's Close under it and the rows' empty socket marks
    (in-game/11);
  - the NPC dialog: held A no longer accepts a quest, the quest reward's icon shows, the HUD is
    hidden and the bars show (town/04);
  - the card at the last row (in-game/18, within a few pixels);
  - the enchanter's page (town/06);
  - a melee skill's description: the requirement line "Right hand requires : melee Weapon", then
    the effects (test/04);
  - the pet to town and its return modal with the bars (in-game/16).
- Found:
  - The enchanter seemed to lose the item (the user did not find it after a "nothing happens"
    result), but it was not lost: the log lists it in the bag's slot 24 (where it was before) until
    the run's end, and the card showed it later. PC's act itself takes the item out of slot 14
    (`0x4e9ca0`, at `0x57b619`) and picks it up into the bag (`0x4e9a30`, `0x57b62a`), dropping it
    at the hero (`0x4f5960`) only if the bag is full; the port's "the item is gone" log line only
    meant slot 14 was empty. To reword.
  - **The gem goblins' and the enchanter's results** still went to the modal and closed the page:
    PC's modal does not exist yet when the act returns (the log shows neither "taken" nor "opened
    again"), so it has to be taken from the frame tick when it appears.
  - **The parchment with chains** behind every modal (the goblin's, the enchanter's, the pet's in
    the mine) is UI: it stands at the same place in all three. It is neither PC's modal window
    (`20_Frame`, effective alpha 0 in `x360_dump_modal-3.json`) nor the 360's modal. Something
    drawn outside the dumped sheet; to find in PC's modal code (`0x54cad0`).
  - With a modal, the 360 hides the HUD (orbs, skill bar, minimap): the port only hides it for
    the NPC dialog.
  - PC's modal text has its own line breaks ("you\n15 Gold\nfrom..."); the 360's flows in two
    lines.
  - The bars: in place they match the 360 (solid, 10 %); the 360 slides them in and out over
    0.2 s (`CInteractiveMenu` setOpen `0x82353068`: TopBar from y -0.1, BottomBar from 1.1, timer
    `+32` = 0.2; per frame `0x821FFD50`), which the port does not.
  - Items on the floor showed no label (the two dropped daggers, the town portal); the pad
    picked them up. Whether PC (and the 360) label loose items without the mouse or Alt is to be
    read.
  - The pet's 3D view in the inventory frames the wolf too close and low (its head under the
    Stats header). The mods agent: one camera (`WardrobeCamXenon`, `sub_823392E8` at
    `0x8233C534`) for hero and pet; the per-unit placement is probably `unit+0x1B0`'s show
    (vtable `+0x24`), unconfirmed; a 360 capture is asked for.
  - The inventory stays open after using a town portal scroll; to compare with the 360.
  - a melee skill shows "Mana Cost: 8" (and the rank's name line) where the 360's capture shows
    none: PC's data or the wrong field (rank `+0x4c`); to check against PC's own description.
  - The PC side panels at the gem goblins and the enchanter, already known.
- The mods agent's captures after the run (in-game/19-24):
  - 19, the pet's sheet (RB from the hero's bag): left panel "<pet name> / Pet", two "No Spell
    Equipped" rows, HP/MP, no level, XP or fame; the lynx whole and small, its feet on the Stats
    header, the viewport's upper half empty. The viewport's rect is the hero's.
  - 20, an item on the floor: the 360 labels it ("A" and its name, gold) above it, next to the
    hero.
  - 21-24, the enchanter: its result is written in its page, not a modal: `resultofenchant`
    (SerifBig, yellow, 357,617 to 721,841 at 1080), between the disenchant chance and Enchant (A);
    the price and the chance go up after each try; the item's new name shows in the bag.
- Fixed after the run (not run yet):
  - The results of the enchanter and the gem goblins: the act marks a result pending; the frame
    tick (`enchant_result_tick`) waits up to 20 s (2 s until QA run 6, 6 s until QA run 20) for PC's modal, takes its text (line breaks
    flowed), presses its Ok, opens PC's enchant menu again with its type (`0x575990`) and closes
    PC's inventory; the shop tick does not close the 360's page meanwhile, and the 360's modal is
    not shown for it. The enchanter's text goes in `resultofenchant`.
  - The HUD (and the pet's) hidden under the 360's modal too (the HUD tick showed it again each
    frame).
  - PC's modal text: a single line break made a space, a blank line kept.
  - The bars slide in and out over 0.2 s as the 360's (`0x82353068`, `0x821FFD50`).
- Still open, read so far:
  - The floor items' labels: PC's label loop (`0x55a8db`) makes a unit's label only for the unit
    under the mouse (its object's `+0x34`, a type-30 unit excepted) or while the show-items key
    (`[0xf60bdc]`, with the toggle at `+0x328`) is down. Which object that is, and so how to give
    the pad's candidate the mouse's place, is not read yet.
  - The parchment: no visible PC window covers it in the dumps (PC's modal `20_Frame` and its
    children at effective alpha 0, no image on its parents). Next: see whether hiding `20_Frame`
    (not only its alpha) removes it, and where `0x54cad0` draws.
- Closed without a run after B3:
  - a melee skill's "Mana Cost" is PC's data: PC's skill loader stores `MANACOST` at the rank's
    `+0x4c` (`0x5e0526`), the field read. The 360's capture is of its own data.
  - The pet's sheet (in-game/19): `petmenu_xenon.layout` is loaded into the character's column
    beside the hero's sheet and shown instead of it on the inventory's page with the pet's bag
    focused: the pet's name, "Pet" (the layout's markup made its colour), HP/MP, the four stats,
    and its two spells (getKnownSpell `0x484ff0` on the pet: icon and name), or no icon and the
    360's empty text (`pet.nospell`, a UTF-8 text in the 360's image, so the converter now reads
    `STRINGS_UTF8` too). Not run yet. The pet's model framing (small, whole, feet on the Stats
    header) is still PC's doll camera: open.
  - Floor items: with Alt PC shows their labels (the user); the 360 shows the candidate's
    (in-game/20). Asked the mods agent whether the 360 shows the others too, and whether its
    inventory closes after a town portal scroll.
- The mods agent's captures 25-27:
  - 25, three items dropped together: all three labelled, overlapping, no spreading; only the one
    A would take has the A; each keeps its own colour (a unique's gold FFDBAE12), a plain one
    white as the candidate and grey (FFCCCCCC) otherwise; after A the next one gets the A.
  - 26-27, a town portal scroll used from the bag: the inventory is closed 0.3 s after A. Whether
    any use closes it (a potion) was asked.
- The floor labels, fixed (not run yet): PC labels the items on the floor only under the mouse,
  or all while KEYMAP_SHOWITEMS (Alt) is down or its "TOGGLE DROPPED ITEMS" action is on
  (CGameUI's update `0x5578e0`; the loop's test at `0x55a942`). The toggle's state is the entry
  `[0xf60c90]` of the vector at `+0x80` of CGameUI's `+0x3c` (`0x5c53d0`), 1 when on
  (`0x557af8`); the port sets it on. The labels keep PC's colour when it is not plain; plain ones
  white as the candidate, grey otherwise.
- The pet's doll: its camera's field of view is widened as the hero's (the hero's factor 1.6, the
  pet's 2.2: a first value from run B3's photo against in-game/19, to measure in the next run).
- The mods agent's captures 28-30 (the use rule):
  - A health potion used from the bag (29): the bag stays open, the stack goes down, the card
    closes and the bar loses Use while the potion recharges (Drop / Swap).
  - The identify scroll (30) has no Use on the 360 (Drop / Swap): an item is identified by A on
    it ("Press A to identify").
  - So the bag closes for the town portal, not for any use: likely the portal's opening closes the
    360's menus. The mods agent is looking for that function; the port waits for it.
  - Open in the port: the bar's Use for a recharging potion and for the identify scroll (the
    port uses PC's useable test, `GOG_IS_USEABLE`); the 360's test before its Use prompt
    (`sub_8236C8D0` near `0x8236E100`, `sub_821D7E20`) is to read.
- The bag closes after the town portal scroll (the mods agent's reading, static): the 360's bag
  Use (`sub_8236BD80`) compares the item's definition (`+0x178`) with the unit index's entry of
  that name (`0x8236BF68`; the name at `0x820C2010`) and, after using it (`sub_822B9A60`) and
  refreshing the sheet, calls its menu's Show(false) (`+0x1c` with 0, `sub_8236F700`); nothing
  else closes the bag on a use. The port reads the item's NAME from its property bag (`+0x178`,
  `0x5c0190`, as PC reads DESCRIPTION at `0x4854dd`) against the converter's `item.townportal`
  and closes the bag after PC's useItem. The name read is logged at each use (not run yet).
- After the user's list (2026-10-23), written, not run:
  - **PC's screens kept shut.** CGameUI's menus, by their RTTI (made at GOG `0x562d96`-`0x563488`):
    CInventoryMenu `+0x2cc`, CStatsMenu `+0x2d0`, CPetMenu `+0x2d4`, CMerchantMenu `+0x2d8`,
    CEnchantMenu `+0x2dc`, CCombineMenu `+0x2e0`, CStashMenu `+0x2e4`, COptionsMenu `+0x2e8`,
    CSettingsMenu `+0x2ec`, CDieMenu `+0x2f0`, CWaypointMenu `+0x2f4`, CDialogMenu `+0x2f8`,
    CQuestDialogMenu `+0x2fc`, CCinematicMenu `+0x300`, CModalMenu `+0x304`, CTipMenu `+0x308`,
    CSkillMenu `+0x30c`, CJournalMenu `+0x310`, CQuestMenu `+0x314`, CInteractiveMenu `+0x318`,
    CFishingMenu `+0x31c`. Out of a shop, the inventory, stats, pet, skill, journal and quest
    menus are closed as they open (PC's hotkeys); the 360's doll, whose viewport PC's closing takes,
    is made again.
  - **PC's interface hidden every frame** while the 360's frame is open: the hidden list kept PC's
    sheets once, but was filled with the same two windows after 64 ticks at a shop and then hid
    nothing more (likely run B3's side panels: CEGUI's dump had them hidden at first).
  - **Use as the 360's:** the bar's Use and A only when the item can be used now (the 360's
    `sub_822B9DA8`; PC's item use `0x4b7470`: no charges left refuses, and each of the item's
    skills is asked of the user by its virtual `+0x118`, false while a potion recharges); none for
    the identify scroll (its unit name, `item.identify`, `0x820C2038`); the card closed after a use.
  - **The label's dark frame:** StaticShared's "frame" (`-24, -9`, `+48, +18` pixels at 720 rows)
    copied into ItemTextButton as fractions of the label's height by the converter
    (`label_frame_relative`), as the A button was.
- The quest marker ("?" or "!" over an NPC) is hidden on the 360 while the NPC's dialog is up
  (town/42 against 43). PC's marker is a 3D element, not CEGUI ("question", "exclamation" and
  their "gray" ones, named at GOG `0x42d910`, `0x42e330`); what shows it is not read yet. Open.
  **Written (not run):** PC's markers are effects on the NPC by name, "QUEST COMPLETE",
  "QUEST INCOMPLETE", "QUEST GIVING", "QUEST PASSIVE" (PC's wide strings `0xabad3c`, `0xabad18`,
  `0xabacfc`, `0xabace0`); the quest manager's update (`0x5f0f50`, the manager `*0xf25320` as
  `0x5efa20` returns it) takes each off with `0x482710` (the unit, the name) and puts the one of
  the NPC's state back. The port takes them off the unit A talked to when the 360's dialog shows,
  and calls that update when it closes.
- **The map's three states on R3** (in-game/31-33, the mods agent's captures: RS cycles the
  corner minimap, the overlay over the play area, none, and round). PC's KEYMAP_AUTOMAP does the
  same three: CGameUI's update (`0x55d380`) calls `0x4ee7c0` on CGameUI's `+0x20` (unless
  `+0x173d`), whose automap (`+0x160`), by its `+0x3c` (shown) and `+0x65` (overlay), goes corner
  -> overlay (`0x4ea910` with 1) -> off (`0x4ea6b0` with 0) -> corner, the state kept in the
  setting `[0xf60c94]`. The pad's R3 (XInput 0x0080) calls it in play. Written, not run.
- The map's look differs: the 360's explored area is a light, opaque grey with a thin rim, PC's a
  translucent grey with a lighter outline. The textures are the same files in both paks
  (`media/sharedtextures/automap*.dds`, same sizes), so the difference is in CAutomap's drawing
  (the 360's `sub_822E3A30` builds its quads, `sub_822E50C0` renders the level's map). Open.
- **The corner map's overlay** (the mods agent): `media/sharedtextures/automapoverlay.dds`
  differs between the paks, the 360's only a faint rim, PC's with its bronze corner and zoom
  buttons; the other automap textures are the same bytes. The converter copies the 360's into
  the output folder (`SHARED_FILES`), which the proxy adds to Ogre's world group after PC's
  archives; whether Ogre then takes it over PC's is for the next run.
- **The cards' descriptions** as the 360's: of the items in both paks, 203 have a DESCRIPTION
  that differs (PC's mouse texts: "Right-click to ...", 171 reworded and 32 with none on the
  360, as the identify scroll). The converter writes `desc.<unit name>` ("~" and the 360's
  text) into x360_strings.txt (`item_descriptions`; the units' .adm read with their 8-byte
  value types 3 and 7, `adm_tree(wide=)`); the proxy gives PC's DESCRIPTION property that text
  for the tooltip call and puts its own back after it, so PC lays the card out with it. The
  texts file's limits were raised (1200 lines, 192 KB).
- The map's look, read so far (open): PC's automap code (`0x4ec000`-`0x4ed100`, its quads at
  `0x4ec599`, its camera at `0x4ece4b`) holds no colour constants, only the camera's distances
  and angles (5000, -90, -225, 4.0). PC names `media/sharedtextures/automapoverlay.png` and
  `automapmask.png` in its code (the paks hold `.dds`): if the overlay is loaded by that name,
  the converter's 360 copy (a `.dds`) will not replace it; the next run's look tells. The fill
  is likely the level's automap tiles (`RAUTOMAP_TILE`, `EditorAutomapTiles`), data in the paks.

**QA run 1 (2026-10-23, town and mine).** Steam's folders UNCHANGED. Fine: R3's three map states,
the floor labels, the town portal scroll closing the bag. Broken and fixed after the run:
- The pet commands closed as they opened: PC's interface, now hidden every frame, took the
  housing (a sheet child the port's list did not name). Every window named with an x360 prefix
  is now the port's.
- No shop opened in town: a shop's menu opens PC's inventory with it, and the hotkeys' guard
  closed that inventory before the shop was seen, closing the shop. The guard leaves PC's
  screens alone while a merchant, enchant, combine or stash menu is open.
- The mine's entrance's label showed from far off: PC's "TOGGLE DROPPED ITEMS", set on for the
  floor items, labels every unit of both its lists on the screen (the level's `+0x7c`, the items,
  and `+0x80`, the objects; GOG `0x55a8e0`, `0x55b263`; without it, the hovered unit only, CGameUI's
  `+0x34` and `+0x2c`). The 360's rule (`sub_821A83C8`, its label loop at `0x821AA9FC`; the mods
  agent's static reading): a unit is labelled when it is the pad's target (unless of type 30), or
  when its 3D distance to the hero is under the root of 20.0 (pool `0x820AA2C0 +0x244`) and it is
  not of type 29 (the breakables); one distance for items, chests, shrines and exits; a "show all"
  input as PC's Alt besides. The port makes the other labels of both lists transparent. Not run.

**QA run 2 (2026-10-23, town and mine).** Steam's folders UNCHANGED. Fine: the mine's entrance's
label only near; the merchant opens; the quest marker of the NPC talked to hidden (the others'
stay, as the 360's town/14); the identify scroll (no Use, the 360's card text); the potion; the
pet commands; the pet's sheet (its model faces front where the 360's lynx is three-quarter: the
user's save has a wolf, the 360's a lynx). Found and fixed after it (not run yet):
- **The 360's modal never showed** (PC's whole parchment modal came up): `modal_skip_until`, 0
  when no wait was asked, was compared as a signed tick difference, which reads 0 as the future
  once the machine's uptime passes 24.8 days (27.2 here). Now tested only when set.
- **The merchant's sides kept PC's panels:** CEGUI had them hidden; PC narrows its world viewport
  while a side panel is open (CGameUI `0x541090`, asking the panels in its vector at `+0x16b8`;
  the client's update at `0x40276c` sets the narrowed viewport; the mouse's and labels'
  projections use the same test), so the strips outside kept the last picture. While the 360's
  frame is open at a shop, `0x541090` answers no (its first bytes "xor al, al; ret", put back
  on closing; patched only if its bytes are as read).
- **The card** 7-8 pixels right of the 360's (measured on the visible border, in the merchant's
  middle column and the inventory's third): PC's tooltip draws its frame about 15 pixels inside
  its root, the 360's 22. The root now goes 10.24 and 12.87 base units into the column.
- **The pause did not pause:** the 360's pause menu now calls PC's pause (`0x541c60` on CGameUI,
  KEYMAP_PAUSE's, flipping `+0x173d`) on opening and closing, PC's "Paused" window (`+0x9c`)
  hidden.
- **Options and Exit to Title** in the 360's pause menu open PC's own pause menu (COptionsMenu
  `+0x2e8`, its setOpen) and press its Settings or ExitGame once it shows, from the frame tick
  (the earlier tries did both at once from the pad's hook and froze).
- **PC's hotkeys:** closing PC's screens as they opened showed them for a frame (the user). Now
  CGameUI's update's tests for KEYMAP_INVENTORY, SKILLS, JOURNAL, QUESTS, STATS and PET
  (`0x55d206`, `0x55d282`, `0x55d2a4`, `0x55d2c5`, `0x55d2e7`, `0x55d318`) are made to always
  jump past the opening, in memory, each only if its bytes are as read.
- Open: the pet's sheet's name is gold (white on the 360), the card's two dividers around an
  empty description, the pet model's three-quarter turn, the minimap's fill (the user: "not
  identical").

**QA run 3 (2026-10-23).** Steam's folders UNCHANGED. The 360's pause menu's Options opened PC's
settings with a window the pad could not leave. The user's decision for the first beta: Start
opens PC's own pause menu (COptionsMenu, CGameUI's `+0x2e8`: Settings, Exit Game, Return to
Game), used with the mouse; Start or B close it; the pad does nothing in the world meanwhile. The
360's pause menu and walking PC's menus with the pad are left for later.
- QA run 4 closed twice at the main menu (a hero and a level are loaded there, so the pad's tick
  runs): the pad asked whether PC's pause menu was open through the shops' isOpen slot (`+0x10`),
  but COptionsMenu is a CMenu, not a CSubMenu: its `+0x10` is another function (`0x574bd0`, a
  vector insert) and `+0x1c` its onClick (`0x591050`). Its open state is its byte `+0x18` and its
  setOpen slot 6 (`0x591200`, thiscall bool). The pad now only asks, and the frame tick opens or
  closes it (the earlier freeze called `0x591200` from the pad's hook). The other menus asked
  (inventory, stats, pet, skill, journal, quest, merchant, enchant, combine, stash) have a plain
  state read at `+0x10` (`+0x30`, `+0x44`, `+0x34`, `+0x1c`, `+0xc8`, `+0x38`, `+0x54`).
- The settings' window from the main menu showed the 360's tip over it, with no way out (QA runs
  3 and 4): its confirmation uses PC's tip layout (prefix `14_`), and its text is none of PC's
  tips. A window of that layout whose text is not one of PC's tips is now left to PC (used with
  the mouse); the main menu has a hero and a level loaded, so "in play" does not tell them apart.

**QA run 5 (2026-10-23).** Steam's folders UNCHANGED. Closed by the user: PC's hotkeys off, Start
and PC's pause menu, R3's map states, the floor labels, the inventory's items (potion, identify scroll, the pet's sheet and commands), the town portal scroll. Fixed after it (not run): the bars
showed over PC's settings screen (its windows have the modal's names): the modal now also needs
CGameUI's CModalMenu (`+0x304`, vtable `0xab7d04`, a CMenu: open byte `+0x18`) open; PC's modal
frame still drew its parchment and chains behind the 360's though transparent: it is moved
10000 pixels off the screen while the 360's shows, and back for the click on its button and on
closing.

**QA run 6 (2026-10-23).** Steam's folders UNCHANGED. Closed by the user: the card's place, the
stash. Found:
- **PC's 3D boards.** The parchment with chains behind the 360's modal stayed with PC's modal
  frame moved 10000 pixels away (the second hypothesis on it, after its alpha 0; stopped there),
  and the merchant's and stash's side panels stayed with PC's interface hidden and its side-panel
  test answering no. They are not CEGUI windows: PC's menus (CMenu: pause, settings, modal, death;
  and its shops' boards) are 3D boards that drop or slide in, animated by an object the menu keeps
  (CMenu's open `0x574f40` calls Entity::_updateAnimation; the object at the menu's `+0x38`, its
  `0x4ca7a0` with an animation name). To hide: that object's scene node or entity, not read yet.
  The user is asked whether to go on now or after the first beta.
- **The enchanter's result:** the enchanting worked, but PC's modal came over 2 s after the act
  (its board drops first), the wait gave up, PC closed the page and the 360's modal showed the
  result (the second try on taking it, after the synchronous one). The wait is now 6 s (not run).
- The black bars over PC's settings are PC's own CInteractiveMenu (`+0x318`), not the port's:
  whether to hide them is the user's call.
- A town portal scroll could not be picked up with the bag full and no stack of them: PC's own
  rule; the pad's choice of item is the 360's (sub_821A4470, no bag test).
- After QA run 6 (not run yet):
  - **PC's 3D boards hidden.** The object a menu animates (its update calls 0x4c9640 on it, then
    Entity::_updateAnimation on its `+0x5c`) is the board: CMenu (the modal, CGameUI `+0x304`) at
    `+0x38` (`0x574477`); CInventoryMenu `+0x7b20` (`0x580f80`), CMerchantMenu `+0x48` (`0x58ac50`),
    CEnchantMenu `+0x50` (`0x5752d0`), CCombineMenu `+0x6c` (`0x5656a0`), CStashMenu `+0x48`
    (`0x5aadb0`), each its vtable's slot 9. Its entity, checked to be an Ogre::Entity by its vtable
    (`??_7Entity@Ogre@@6B@`), is made invisible (MovableObject::setVisible) while the 360's modal
    or shop frame covers it, and visible again after.
  - **PC's own bars** (CInteractiveMenu, `interactivemenu.layout` Root/TopBar/BottomBar) made
    transparent: the user saw them over PC's settings.
  - **The card's dividers** made transparent: the 360's card has the windows but draws nothing
    there (in-game/10, 30: the background at their rows).
  - **The pet's name** white, as the 360's (in-game/19).
  - **The pet seen three-quarters:** PC's pet camera (made at `0x5980e7`: (0, 1, `[0xaa75f4]` 3.5)
    looking at (0, 1, 0)) turned -55 degrees about (0, 1, 0) while the pet's sheet shows (the 360
    turns the model, not its one camera at (0, 1, 4.5)); a first angle, to measure.
- Moved to "nice to have, after the first version" (the user, 2026-10-23): the minimap's fill
  (the 360's lighter, opaque grey).
- **The right stick zooms** (the user: the 360's right stick zooms, as PC's wheel). PC's zoom is
  its camera controller's (the client's `+0x10`, GOG `0x5e65f0`): the distance `+0x1c`, moved by
  the wheel times -0.01 or 1 per frame with KEYMAP_ZOOMIN/OUT, kept from 14 (`0xaba608`) to
  `+0x60` times 28.5 (`0xaba60c`), while `+0x64` is set and `+0x6c` is not. The pad moves it by the
  right stick's y (up in), outside its dead zone. The 360's rule (`sub_821EBA48`, 0x821EBF54; the
  mods agent): the distance moves by sThumbRY / 32767 times -0.35 per frame (no time step),
  kept from 14 to 28.5 times the level's multiplier (its camera controller at `0x83559498`:
  `+0x18` the distance, `+0x5C` the multiplier, `+0x60` enabled, `+0x68` the dialog's zoom); so 21
  per second at its 60 frames, the port's rate by the clock. The user found it right (QA run 8).

**QA run 7 (2026-10-23).** Steam's folders UNCHANGED. The main menu was not drawn and its
background hero took the pad: PC's bars were looked up by their windows' names (TopBar,
BottomBar, Root), and PC's main menu frame has windows of those names, so it was made
transparent. Now CGameUI's CInteractiveMenu itself (`+0x318`, vtable `0xab731c`, its root at
`+0xc`, checked by its name) is. And the pad does nothing out of play (no PC HUD: the main menu
has a hero and a level loaded behind it).

**QA runs 8-10 (2026-10-23).** Steam's folders UNCHANGED each time. Closed by the user: the right
stick's zoom (then set to the 360's rate), PC's bars over the settings, the card's dividers, the
pet's sheet (name and three-quarter view), fishing (the ring's bite is the 360's 0.25-0.4 s window,
`sub_822D21F0`; catch, B without the skill on B, feeding the pet), the modal without the
parchment.
- **PC's 3D boards hidden** (run 10): the entity check had used Ogre's export `??_7Entity@Ogre@@6B@`;
  the vtable at an Entity's offset 0 is `??_7Entity@Ogre@@6BShadowCaster@1@@` (MovableObject derives
  from ShadowCaster first), so every board had been refused (the run 9 log: "not an Entity's
  vtable"). With it, the modal's parchment and the shops' boards are gone.
- **The shops' world stayed narrowed** (run 10: blue strips where the boards were): PC has two side
  tests, `0x541090` and `0x541130` (and `0x549b70`, both); only the first was patched. Both are now
  (not run).
- Still to run: the enchanter's and gem goblins' result in their page (the 6 s wait); the
  transmuter. Seen: the minimap stays under the 360's modal (the 360 hides it, in-game/16).
- Written this night (not run): `tools/install.py` (stores, 360 copy, all prompt styles, ini, proxy;
  GOG's build only), the pad prompts by the connected pad (`proxy/setup.c`, HID vendor and product),
  the update check (GitHub's latest release, offered at the next start; run 10's log: "no answer"
  as there is no release yet), the GPL-3.0 license.
- **PC's menus with the pad** (the user, for the first beta; written, not run): out of play (the
  main menu `mainmenuframe`: ContinueLast, NewGame, ContinueGame, Settings, ExitGame; character
  load and create, difficulty) and, in play, PC's pause menu (COptionsMenu's root, CMenu `+0xc`):
  their visible buttons (PC's windows of a Button type) in a circular list from the top one down;
  up and down (d-pad, or the left stick with repeat) move the focus, shown as PC's own hover (the
  CEGUI cursor, hidden, injected at the button's centre); A clicks there. The CEGUI mouse cursor is
  hidden everywhere except while PC's settings screen shows (its Apply and Cancel), kept so each
  frame as PC shows it again.

**QA runs 11-21 (2026-10-23).** Steam's folders UNCHANGED after every run.
- **PC's menus with the pad** (closed by the user: main menu, settings, character creation and
  load, difficulty, pause, B back).
  - PC reads Windows' cursor every frame (`0x4e7330`: GetCursorPos, ScreenToClient) and injects it
    into CEGUI with a 1 px move for the hover (`0x4016c0`), so an injected position lasted one
    frame (run 12). The focus puts Windows' cursor on the window (ClientToScreen, SetCursorPos):
    PC hovers it itself.
  - The windows: types ending Button, Editbox, Checkbox, CharacterSlot, Slider, Combobox, under the
    sheet (out of play, PC's in-game sheet skipped: its HUD buttons stay visible while a game
    loads), PC's pause menu's root, or the settings' own root; collected every frame, at most 64.
  - The direction moves to the nearest window that way (distance along plus twice across; none,
    around on the same line): the main menu is a row, the pause a column, character creation two
    columns.
  - A on a slider or drop-down starts changing it with left and right (A, B, up, down end it); a
    drop-down by `setItemSelectState`, its text set and `onListSelectionAccepted` fired; placed by
    its edit box (CEGUI's Combobox spans its list).
  - B clicks the screen's Cancel or Back. A popup (a slot holding listed windows: the load's
    DeleteConfirm) keeps the pad on its windows. A character slot is clicked as the focus comes
    (PC's highlight, as the 360's moving selection).
  - PC's settings told by being on the sheet (PC closes a menu by detaching it; "visible" stayed
    true, run 13). In play, the play pad is off under them and the buttons they used are kept from
    it until let go (B closing them cast the skill, run 20).
- **The mouse**: PC's cursor is Windows', set by `0x547ca0` (thiscall; LoadCursorW of its
  resources 0x6a-0x6d by `+0x106c`, none when `+0x60`). Hooked (7 bytes to a trampoline): none but
  in PC's settings and character creation (told by its Destroyer: its CreatePlayer shows only with
  a name).
- **Crash** (run 19): the pad's A on an item read its class after `pickup` (an item in reach is
  taken within the call): read before now.
- **The quest dialog** (run 18): PC's cinematic menu has a DialogTextFrame too and took the search;
  the dialog is found by its Portrait as well.
- **The enchanter and gem goblins**: their act (`0x57a800`) calls PC's modal (`0x54cad0`, title and
  text std::wstring by value, an int; callee pops 0x3c) and it showed seconds later. Hooked during
  the act: the text taken for the 360's page, PC's modal not opened (run 21: taken, but the page
  showed nothing; the page's window and text are logged now to see why).
- **The transmuter** (run 20): with no recipe for its places the 360 plays the sound 24 and redraws,
  no message (`sub_82379058`, its recipe search's 0 to `0x82379990`); the port does the same,
  told by its places unchanged.
- **Tabs** (the user's question): the 360 keeps one menu for the merchants and one for the stashes,
  each with its filter (`sub_8236A488`); buying (`sub_8236B578`) and storing (`sub_8236BA20`) do
  not change a tab. The port does the same.
- **The bag** (run 21: weapons refused at "23 of 63"): PC's hero and pet (unit kind 0x57) get three
  sections of 21 slots (`0x49f85f`: CInventory `0x4e9820` with 21, its first section from slot 19;
  `0x49f880` and `0x49f88f`: addSection *(OT)* `0x4e9a00` for the types 1 and 2); the free slot is
  looked for in the item's section only (`0x4e85b0`, `0x4e8410`, `0x4e84a0`). The 360 gives a unit
  one section of 50 (`sub_822883F0`: `sub_822E0308` with 50, 0 for type 170). The "63" was PC's
  three sections. `bag=360` (an install option, the user's) patches PC at load to one of 50.
- **The installer** (`tools/install.py`), as the recomp's first start: the XBLA package (STFS,
  `tools/stfs.py`; title 58410A7E, content type 000D0000) or an extracted folder; `default.xex`
  decrypted and decompressed by `tools/xex` (the recomp's `xex_dump.py`, 360tools' LZX, a plain
  Python AES); the bag asked with its warning, saves backed up (the game's prefix and those beside
  it); conversion into the game's folder through `.partial` folders, the work folder in the game's
  folder (a /tmp tmpfs's quota filled up); uninstall. Run end to end on a copy of GOG's folder: the
  conversion is identical to the one the game uses. The update check was taken out (the user's
  decision).

## 18. The proxy built without the game's DLL: `proxy/cegui_exports.txt` (2026-10-23)

The owner's decision (2026-10-23): the installer becomes one executable per system, with the proxy
built in CI and published in the release, so the export table it forwards is kept in the
repository as names only.

**What the file holds.** One line per export of `CEGUIBase.dll`: its ordinal, its source and its
MSVC mangled name, in the order of the DLL's name table. No code, data, address or text of the
game: every name's qualified name (the mangled name up to its first `@@`) ends in namespace
CEGUI, a member of a class there or a free operator of CEGUI; at most ten header lines before the
first entry, nothing else. `tools/cegui_exports.py` checks it when reading the file and
`tools/check_no_game_files.py` in CI. That check accepts an export table in this file only: any
other tracked file with more than five lines holding a mangled CEGUI name with an ordinal or a
forward (this list's form with any spacing, a `.def`'s, a dump's), or more than 400 lines naming
one, fails CI, as does a `.def` or `.lib`.

**Where it comes from.** `tools/cegui_exports.py make GAME_DLL OFFICIAL_DLL`, run on 2026-10-23:

- The game's DLL: GOG's `CEGUIBase.dll` (kept as `CEGUIBase_orig.dll`, SHA-256 `360bff56…ffa6d1`),
  3839 exports, ordinals 1 to 3839. Steam's `CEGUIBase.dll` (SHA-256 `b3464338…dfe28`, read only)
  exports the same 3839 names with the same ordinals (`cegui_exports.py check` prints "same
  exports" for both).
- The CEGUI version: 0.6.2. The DLL's assertion paths name `CEGUI-0.6.2\src\…`.
- The official build compared: `bin/CEGUIBase.dll` of CEGUI's own SDK,
  `CEGUI-SDK-0.6.2b-vc9.zip` from the project's SourceForge files (zip SHA-256
  `75e1aab9…5737c7`, DLL SHA-256 `95305f53…274cfec`), 3835 exports. 3828 of the game's names
  are in it: marked `cegui`. The other 11 are Runic's own, marked `runic`: seven
  `Font::drawText` overloads with one more `bool` argument than CEGUI's (which replace the
  official seven, absent from the game's DLL) and `Font`'s colour trigger members
  (`setColourTrigger`, `setColourUnTrigger`, `setColourTriggerEnabled`,
  `isColourTriggerEnabled`). The ordinals are the game's: the official build numbers 2569 of
  the common names the same way, the rest are shifted by Runic's additions.

**How it is used.**

- `tools/build_proxy.py build [REAL_DLL] OUT` writes the proxy's `.def` from the list. With
  REAL_DLL, it first checks that the DLL exports exactly the list (names and ordinals) and that
  the folder's `Torchlight.exe` is the target's build. Without it (CI), only the names the proxy
  resolves in `CEGUIBase_orig.dll` are checked against the list; those of `OgreMain.dll` and
  `msvcp90.dll` need the game's folder.
- The installer checks the game's real `CEGUIBase.dll` against the list before anything else
  (`install.check_real_dll`). A DLL that differs would leave the game calling into exports the
  proxy does not forward.
- The link is reproducible (`lld-link /timestamp:0`): two builds give the same bytes. The proxy
  built from the list is the one built from GOG's DLL, and differs from the one installed for
  the last runs only in the PE timestamp's four bytes.
- CI's `proxy` job builds both targets from the list with LLVM 21 (apt.llvm.org's) and prints
  their SHA-256. Its first run (2026-10-23) gave the hashes of the local build (Ubuntu's clang
  21.1.8): GOG `54dbc7d4…31d7c62b`, Steam `dfb727c2…9a3617`. `build_proxy.py` takes LLVM 21's tools
  first (`clang-cl-21`, then `clang-cl`).

## 19. Epic's build: GOG's (2026-10-23)

The owner's copy of Epic's Torchlight (downloaded, in `~/Descargas/epic/Torchlight`, read only):
every game file is GOG's, byte for byte (`Torchlight.exe` with GOG's timestamp `0x4FFC6815`, build
17870 in its `BuildVer.txt`; the DLLs, `Pak.zip`, music, programs; even GOG's `goggame-*.dll`).
Epic's folder adds its uninstaller (`unins000.*`), a shortcut and `GameuxInstallHelper.dll`; GOG's
adds its `goggame-*` data files and `webcache.zip`. Its `CEGUIBase.dll` exports the list
(`cegui_exports.py check`).

The installer was run on a copy of that folder (the large files linked, `CEGUIBase.dll` copied)
with the owner's extracted 360 copy: told as GOG's build; the converted interface identical to the
one GOG's game uses; the proxy identical to the one built from the repository; the uninstall left
the folder's files with the hashes they had before. The owner's decision: Epic is marked supported,
its executable being the one played on GOG. A copy outside Legendary's, Heroic's or the launcher's
lists is not found by `install.py list`; it is given with `--game`.

## 20. The 360's bag on the hero (QA run 25, 2026-10-23)

The installer's two runs on GOG's folder (PC's bag, then the 360's): the converted interface and
the proxy identical to the repository's; the saves untouched, backed up (identical copies) with
the 360's bag; `save_folders` raised a TypeError when called without a home (fixed, tested). Run
25 with `bag=360` and the owner's test hero: the pet's bag 0/50, but the hero's still PC's (82
slots, sections from 19, 40, 61), no item lost. The patch covered the unit ctor's branch of kind
0x57 only, which is the pet's; the hero takes the ctor's last branch (`0x49f8ee`, one section of
21) and its own ctor `0x4dbc40` adds the types 1 and 2 with 21 each (`0x4dbc8d`, `0x4dbc9c`). The
patch now makes both pushes 50 and leaves the four adds out (any other unit of the last branch
gets 50 too, as every unit with a bag on the 360). Steam: `0x49d752`, `0x4d925f` (verified) and
`0x4d926e` (checked: the next call, `push 0x15; push 2; call 0x4e7370`, Steam's addSection, read in
its disassembly). Not run yet.

## 21. Items placed at load, and the save tests on copies (2026-10-23)

**Static reading (GOG).** A unit's saved data is applied by the function around `0x4acc3d`: for
each saved item (the list at `+0x1e8`, each with its slot at `+0x88`) the item is made and put
with `0x4e9480(item, slot, 1)` at `0x4acd4b`. For a slot above 19, `0x4e9480` takes the item's
type (`0x4e8410`), its section (`0x4e84a0`) and checks the slot is in it (`0x4e77a0`); when not, it
returns 0 and the loader destroys the item (`0x4acd5b`, its vtable's first entry with 1). So an
item saved in a slot that the current bag does not have for its type (69 to 81 with the 360's bag;
another type's section with PC's) is lost at load. The owner's plan: placed again at that call
(a free slot of its section), and the installer refusing a bag change that would not fit.

**`save_root=DIR`** (x360_ui.ini, a diagnosis, off by default): the game's application data
folder made DIR (the executable's import of `SHGetSpecialFolderPathW` replaced, found by name;
the game asks for CSIDL_APPDATA through its one wrapper `0x5cc6c0`), so a test runs on copies in
DIR's `runic games\torchlight` and the user's own folder is neither read nor written. Not run yet.

**QA run 28 (2026-10-23, the copy of the test hero, its health potion's slot made 70 with the 360's
bag).** The potion was not in the bag after loading (the HUD's count gone) nor in the file after
saving: the static reading confirmed. `backup.tmp`, written at load, is the loaded file as it was
(a copy, not a save of the new state); the save itself was written on leaving. (Run 30: a level change, the town portal, writes it too; the stop comes at the first frame in play, before any.)

**The bags, read (GOG).** A bag's sections have types (`+0x48`, codes into `0xaa9530`): 0 any item
(ISA 31), 1 spells (129), 2 fish (124), 3 weapons (8), 4 armour (13); `0x4e8410` gives an item the
last section whose type it is, else the first. The hero's and the pet's (PC): 21 any, 21 spells,
21 fish; with `bag=360`, 50 any. A merchant's (ISA 41, the ctor's branch 0x29): 42 any, 42
weapons, 42 armour. The ctor's branch of ISA 128: one of 42. The shared stash (`0xe40900`, its
CInventory made at `0x52df16` and again by its loader at `0x52e177`): one section of 42, owner
none, the same with either bag; its loader (`0x52e33e`) destroys an item PC refuses as the unit
loader does (`0x52e34f`). Which ISA the personal stash is, and so whether the ctor's last branch
(made 50 by `bag=360`) reaches it, is to be read in the next run (the hook logs each unit's
refusals).

**The hook** (`x360_load_hook_install`, either bag): the loader's call at `0x4acd4b` (Steam
`0x4aaae5`) goes through `load_put_hook`. For the hero's (ISA 28) and the pet's (ISA 87) bag slots,
an item PC refuses goes to the first empty slot of its type's section; with none it is kept aside
and, at the first frame in play (the pet waited for up to 10 s), put into the other one's bag;
any left stops the game with a message and `TerminateProcess` (no game code runs after it, so no
save). The pet is waited for without a clock (the owner: a slow machine must not stop the game):
until it exists or the world is ready without it (the 360's HUD up, then 60 frames counted); with
no pet the message says so. Pending (the owner, 2026-10-23): the installer's check of every save
before a bag change (a read-only parser of the saves), left for later since the stop loses nothing. The personal stash is left out of the order (the owner's rule when it is not known to exist
at load: a unit of the town's level, not the hero's). Not run yet.

**The round trip, QA runs 29 to 32 (2026-10-23, `save_root` on a fresh copy of the test hero, the
owner's two save folders' hashes the same before and after every run, Steam's folders
UNCHANGED).**
- Run 29 (PC's bag): the title loads the last character (twice, four times in run 30) before the
  main menu; its items kept aside were placed into the title's pet, and the real hero's again:
  duplicates (the copy thrown away unsaved). Fixed: nothing placed out of play (`hud_in_play`),
  an item kept aside whose unit is gone dropped as PC's loader would.
- Run 30 (PC's bag, stage S1): four items the 360's bag had put in the spell section's slots
  (41-44), the hero's main section full: into the pet's bag (19-22), once each; the title's
  copies dropped. The console's 20 spells and a gem (socketed in the mace); the stash is
  ISA 128 with one section of 42 (its slots 61): the ctor's branch of 42, which `bag=360` leaves.
- Run 31 (the 360's bag, S2): nothing moved at load; the bag filled to 50 at a merchant.
- Run 32 (PC's bag, S3): a spell from 63 to 57 in its section; seven items with no room in the
  main section into the pet's bag (23-29).
- `tools/save_items.py compare S2 S3`: the stash the same; the hero's and the pet's 58 items
  pair off by name and affixes, 57 byte for byte (slots and the save's instance ids aside); the one
  left, the axe, was the hero's last record in S2 and runs on there into the hero's trailer
  (INNATEARMOR twice, then the level), which the partial reading does not cut. The merchants'
  stock differs (restocked between runs).

**The stop, QA runs 33 and 34 (2026-10-23, `savetest3`: a fresh copy, the owner's folders' hashes
the same after both runs, Steam's UNCHANGED).** Run 33 (the 360's bag): the hero's bag filled to 50
(30 blades by the console), saved (N1). Run 34 (PC's bag): the title's loads left to PC, the
hook neither moving nor keeping aside anything (each refusal logged as the title's); the player's
load: 21 items into the pet's main section, 6 blades (slots 63-68) listed as fitting nowhere,
"6 item(s) fit nowhere", the stop. The save afterwards byte for byte N1's (`0.SVT` and
`sharedstash.bin`, their times unchanged). The message box showed blank over the full-screen
game (accepted with Enter): the game's window is now minimized first, and the text says Enter or
OK; the pad does not reach a Windows box.

**The saves, read (GOG), and the stop as PC's modal.** Of the executable's `_wfopen_s` calls two
open a file "wb": `0x41762d` and `0x52e4f5`. The first is in `0x417380` (thiscall, two arguments,
ret 8), which writes `backup.tmp` and renames it into the `.svt` (`_wrename`, `0x417fad`); the
second is in `0x52e3f0`, which writes `sharedstash.bin` and whose one caller is `0x4173ce`, inside
`0x417380`. So every save of the character and of the shared stash goes through `0x417380`; its
nine callers are `0x402559`, `0x419a66`, `0x419d62`, `0x41a4f4` (in `0x419b70`, twice),
`0x41b919`, `0x41ba70`, `0x4daae1`, `0x4daaf7` (in `0x4daa80`, twice) and `0x56d40d` (the console,
`0x56cd00`). The other files opened for writing ("w,ccs=": `0x5c4569`, `0x5c5608`) are text
files (the settings, the logs). The proxy hooks `0x417380` (Steam `0x417110`): while a stop is
pending it returns at once and logs "a save refused". The stop itself: saves held back, the text
in the log and in `x360_stop.txt` beside the game, and PC's own modal opened with it
(`0x54cad0` on CGameUI, the strings made with msvcp90's `basic_string<wchar_t>(const wchar_t *)`,
`0xa85418`), which the 360's modal shows and which takes the pad, keyboard and mouse as PC's modal
does; when it closes, `TerminateProcess`. QA run 37: the 360's modal showed the text (A OK), A closed the game, the save and `sharedstash.bin` byte for byte N1's, the owner's folders' hashes the same, Steam's UNCHANGED; no save was attempted meanwhile (none logged). The text named the class (`unit_name`): it now takes the shown name (`+0x420`), turned to UTF-16. `hold_saves=1` in x360_ui.ini (a test, off by default) holds every save back from the start, to try the hold by changing level and leaving. QA run 38 (the 360's bag, `hold_saves=1`, `savetest3`): a town portal there and back and the pause's exit: five saves refused in the log (two at each level change, one on leaving), the save folder's files byte for byte as before, the owner's folders' hashes the same, Steam's UNCHANGED.

**The warning without the mod (the owner, 2026-10-23).** In three places, in English (the
installer's only language): the installer's question for the 360's bag (`BAG_WARNING`, with
`NO_MOD_WARNING`: without the mod, uninstalled or removed by a store's verify, repair or update,
the original game deletes the items that do not fit PC's bag; before uninstalling, PC's bag, then
load and save each character); the uninstaller with the 360's bag installed (PC's bag instead, the
default and `--yes`'s; or uninstall anyway, confirmed or `--anyway`, the saves backed up first);
the README's "The 360's bag and your saves".

## 22. Nintendo's layout: A and B swapped (2026-10-23)

The owner's request, after the recomp's `swap_confirm_cancel` (the mods agent's reading of its
`src/mods/pad_prompts`, `src/game_menu/mods_install.cpp`, `src/settings/host_settings`: only A and
B swapped, X and Y kept; the prompts follow, from a "nintendo-swapped" set of icons; off by default,
the bottom button confirming as on the 360; its checkbox greyed for Xbox and PlayStation prompts).
Here:
- **The setting:** `x360_settings.ini` in `%LOCALAPPDATA%\x360ui` (beside the DLL before 2026-10-23), `swap_confirm_cancel=1`, the mod's own file
  (not PC's settings), written when the checkbox changes; removed by uninstall.
- **The checkbox:** made in PC's settings screen when it shows, of PC's own checkbox type, in the
  parent of its last checkbox and one row below it; its text says it needs a restart. Enabled only
  with a Nintendo pad found and no Steam Input pad (the owner's rule; the recomp enables it by the
  prompts setting). PC's settings pad navigation reaches it as any checkbox.
- **Which pad:** the HID devices' vendor (Nintendo 057E); Steam Input's virtual pad (Valve 28DE) is
  noted and the search goes on for a physical one. With Steam Input's pad present the swap is
  never applied (Steam's own Nintendo layout option swaps already) and the checkbox says to use
  Steam's option.
- **The swap:** the pad's A and B bits exchanged where the proxy reads XInput (`pad.c`), so play and
  every menu see it; active while the setting is on and the pad is Nintendo's, checked again at
  each pad connection (an Xbox pad connected later is not swapped).
- **The icons:** the installer converts a sixth folder, `x360ui-nintendo-swapped`
  (`convert_x360_ui.py --prompts nintendo --swap-confirm-cancel`), chosen at start with the setting
  on and a Nintendo pad.
`pad_detect=nintendo|steam-nintendo|xbox` in x360_ui.ini (a test, off by default) makes the search report that pad, for trying it with the owner's 8BitDo (no Switch mode). Pending: the detection itself with a real Nintendo pad, with and without Steam Input (whether Proton shows the physical pad's HID device beside Steam Input's is not known). QA runs 41-45 (GOG, the owner's 8BitDo): with `pad_detect=nintendo` the checkbox enabled (run 41 found it over one of PC's: moved to the free row under the resolution), set, and at the next start the swapped icons' folder and A and B swapped in play; without `pad_detect`, the setting still on: Xbox's icons, no swap, the checkbox greyed and set; with `pad_detect=steam-nintendo`: Nintendo's unswapped icons, no swap, the checkbox greyed with its Steam Input text. Steam's folders UNCHANGED.

## 23. PC's UI seen at a load, and its modal's board (2026-10-23)

- **At a load** PC's bottom HUD and pet HUD showed for the first frames of play (the owner): the
  HUD tick made them transparent only once play was entered. They are now made transparent as PC
  loads their layouts at the game's start (`x360_pc_layout_loaded`, from `hook_load_layout2`:
  BOTTOMHUD, PETHUD; PC changes their visibility, never their alpha). QA run 46 (before: the owner
  saw it), runs 47-48 (after): a recording of the screen's bottom middle, 13 734 frames over the
  menu, the load and play, no frame with PC's orbs; the owner saw none.
- **PC's modal's board** (a 3D sign, CModalMenu `+0x38`) was hidden only while the 360's modal
  showed: its fall before it and its rise after it were seen (the pet's return). It is now kept
  hidden always, the 360's modal showing every one of PC's modals. QA run 49: the pet's return, no board falling nor rising (the owner).

## 24. The installer in C++, batch 2: the readers (2026-10-23)

The owner's plan (2026-10-23): one executable per system (Windows `.exe`, Linux AppImage), the
Python installer kept until the C++ one gives the same files byte for byte, then removed.
`installer/` (CMake, C++17):
- `src/stfs.cpp`: tools/stfs.py's reading, step for step (its generator's next-block reads
  included, so a damaged package fails the same way).
- `src/lzx.cpp`: tools/xex/lzx_decompress.py ported (MIT, and derived from libmspack's LZX
  decoder, originally LGPL-2.1, under GPL-3.0 per its section 3: the notices in
  `installer/third_party/NOTICES.md`), Python's integers as 64-bit ones.
- `src/xex.cpp`: tools/xex/xex_dump.py; AES-128 CBC by tiny-AES-c (The Unlicense, commit
  `23856752`, one code path on every system, CBC only).
- `src/main.cpp`: for now `dev stfs`, `dev xex-dump`, `dev aes-cbc`, whose output is the Python
  tools'. Linux links libstdc++ and libgcc in (only libc needed); Windows the static runtime.
- Tests: `installer/tests/core_test.cpp` (AES's FIPS-197 and SP 800-38A vectors, an uncompressed
  LZX block, a synthetic XEX, synthetic STFS packages contiguous and chained), and
  `tests/test_installer_cpp.py` (the binary against stfs.py and aes128.py on synthetic data; run
  when X360UI_INSTALLER names the binary). CI's `installer` job builds and runs both on Linux and
  Windows.
- Against the owner's copy (2026-10-23): `dev stfs` lists the package as stfs.py does and
  extracts its 57 files identical to stfs.py's; `dev xex-dump` writes the image identical to
  xex_dump.py's (22 806 528 bytes, base 0x82000000), in 0.3 s against 17 s.

**Batch 3: the converter** (`installer/src/convert.cpp`, `prompts.cpp`, `pytext.cpp`):
tools/convert_x360_ui.py and tools/pad_prompts.py ported step for step. Their image code is their
own and is ported as it is: the PNG decoder (miniz's inflate for zlib's), the DDS decoder and
encoder in integers, the icon's fitting in doubles in Python's order (no fused multiply-add,
Python's round half to even). The re patterns are matched by hand (`pytext.cpp`) with Python's
rules; where Python decides `\w`, `\b` or re.I by Unicode tables on a character outside ASCII the
C++ stops (NonAscii) instead of guessing; the same for `str.isprintable` outside Latin-1's signs and common punctuation, for `int()` on any character outside ASCII or past 18 digits, for `str.isprintable` only where Python asks it (a text under 100 characters), and for a zip entry's name outside ASCII that a filter would take (or one in cp437, without the UTF-8 flag, an Info-ZIP Unicode Path field (0x7075) taken as zipfile 3.12+ takes it: its UTF-8 name when its version is 1 and its CRC is the raw name's (the owner's 360 pak has 3 035 such entries); every entry's name checked when the zip opens, as zipfile does (a corrupt extra field, a flagged name not UTF-8: the zip refused), the records read from the archive's start (data before it allowed). Not matched: zipfile's check, on reading, that the local header's name is the central one's; a name flagged UTF-8 that is not stops, as Python refuses the zip). A short DDS stops as Python's struct.error does; a 32-bit mask wider than 8 bits gives Python's ValueError text in the report. Files written in text mode take the system's line end,
as Python's. The 360 code's text addresses (`convert_strings.inc`) are generated from STRINGS
and compared with it by a test. The icons of `data/prompts` are built into the executable.
- Against the owner's copy (2026-10-23): the six variants (xbox, playstation3, 4, 5, nintendo,
  nintendo-swapped) converted with the image and PC's pak, every file of each the same bytes as
  the Python converter's (the layouts, imagesets, looknfeel, scheme, textures with the icons,
  the report and x360_strings.txt); no NonAscii stop; 0.6 s against 1.2 s (run again after each review's fixes, the last on 2026-10-23: the same).
- `tests/test_installer_cpp.py`: a synthetic 360 pak (ARGB, DXT1, DXT3 and DXT5 textures with
  prompt images, the label's look, UTF-16 and UTF-8 with a BOM, tips and item .adm files, a PC
  pak, an image with texts) converted by both for the six variants, every file compared; the
  NonAscii stop; the strings table. CI runs them on Linux and Windows, each against its own
  Python (line ends).

**Batch 4: the installer itself** (`installer/src/install.cpp`, `game.cpp`): tools/install.py's
install, uninstall and list, with its questions, texts and results, and what the owner asked of
the C++ one:
- **Permissions (Windows).** The executable's manifest is asInvoker, so Windows never redirects a
  write it refuses (no VirtualStore). Once every question is answered and before anything is
  changed, the game's folder is tried for writing (a file created and deleted on close); when that
  fails the installer runs itself again elevated (UAC, `runas`) with the answers, and a declined
  prompt ends it with "nothing was changed". On Linux a folder it may not write ends it the same
  way (no elevation).
- **One step.** The six interfaces are converted into `.partial` folders first; then the old
  folders are put aside, the new ones put in place, the ini written, the proxy installed (as
  build_proxy.install) as the steps of one change, undone in reverse if any fails (the old
  files back from an undo folder). Uninstall the same: the real DLL back and the mod's files put
  aside, then removed. `X360UI_TEST_FAIL_AFTER_PROXY` makes a test's run fail at the end.
- **The bag.** The 360's bag's warning (with uninstalling and the stores' verify, repair or
  update), the saves backed up first; the uninstaller with the 360's bag offering PC's bag (the
  default) or uninstalling anyway with the saves backed up; once the 360's bag was used, with PC's bag
  back, a question whether each character was loaded and saved with it, and a backup.
- **The proxy.** Given with `--dll`, else the release's built in (`-DX360UI_PROXY_GOG`,
  `-DX360UI_PROXY_STEAM`). The real DLL is checked against proxy/cegui_exports.txt (built in).
- **Questions.** Windows' own boxes and file dialog; on Linux zenity or kdialog when there is a
  display; else the console (`--console`); `--yes` the defaults.
- **Tests** (`tests/test_installer_cpp.py`, fake folders only): a Wine prefix's GOG folder with
  saves beside it, installed with PC's bag and the 360's by both installers: the same files, ini
  and backups; uninstalled by both back to the folder as it was; a Steam library (Steam's build,
  experimental in both) and a Legendary (Epic) install: `list` the same and the same files
  installed; a failed install and a failed uninstall leave the folder as it was; a folder without
  write permission (Linux) and one whose ACL denies writing (Windows, the elevation declined):
  nothing changed.

- **After the reviews.** The elevated run gets the first run's answers, its save folders (that
  account's, not the administrator's) and its yes to no backup, and writes its result or error to
  a file the first run shows in a window. Errors are shown in a window whenever the installer
  runs with its windows. The proxy is checked before the conversion. A rollback that cannot put
  an original back keeps its undo folder (marked `x360ui-keep.txt`, left by uninstall) and says
  where it is; leftover undo folders without the mark are removed on uninstall. The ini is read as
  Python reads it (UTF-8 ignoring errors, universal newlines, Unicode spaces). Backups keep their
  times, and a second backup in the same second gets a suffix instead of failing.

**Batch 5: the release** (`.github/workflows/release.yml`). A pushed tag `v*` makes a draft
release (the only job that may write); a manual run, or a pull request that changes what the
release is made of, makes only the artifacts.
- **Built.** GOG's and Steam's proxies from proxy/cegui_exports.txt (LLVM 21, as CI); the
  Windows executable (MSVC, static runtime) and the Linux one (Ubuntu 22.04's GCC, libstdc++ and
  libgcc inside it, so it needs only glibc 2.35 or newer) with both proxies built in. The AppImage
  (`packaging/linux/make_appimage.sh`) is made by appimagetool 1.9.1 with the type2-runtime
  `8f39b89`, both pinned by SHA-256 (`packaging/linux/get_tools.sh`); its notices are
  `packaging/linux/APPIMAGE_NOTICES.md`, inside the image with the installer's.
- **Checked from the artifacts, where there is no Python or compiler** (the jobs fail if
  python, python3, clang, gcc, g++, cc or c++ is there; on Windows also py, clang-cl and cl): the
  AppImage in clean `ubuntu:22.04` and `archlinux:latest` containers
  (`packaging/check_release.sh`, extracted and run: containers cannot mount it), the executable in
  a Windows Server Core container (`packaging/check_release.ps1`). On `tests/release_fixture.py`'s
  synthetic games: GOG's in a Wine prefix with the 360's bag and Steam's (experimental) with PC's
  bag, and on Windows also GOG's in Windows' own layout (its saves in `%APPDATA%`); each install
  puts the release's own proxy in place and backs the save up unchanged, each uninstall leaves
  the folder's folders, files and hashes as they were (hidden ones too); `list` in an empty home
  finds nothing. The AppImage is also started mounted (FUSE) on a runner host.
- **Published.** The two installers, the two proxies, the licence and the notices
  (`LICENSE.txt`, `THIRD_PARTY_NOTICES.md`, `APPIMAGE_NOTICES.md`), the AppImage runtime's LGPL
  source (`fuse-3.15.0.tar.xz`, `type2-runtime-8f39b89.tar.gz`, `packaging/linux/get_sources.sh`,
  pinned by SHA-256) and `SHA256SUMS` of all, in a draft release that the owner reads before
  publishing (the README's support claims stay as played).
- **The type2-runtime source** is GitHub's archive of that commit, pinned by its SHA-256: GitHub
  does not promise those bytes stay the same, and if they change the release job stops (nothing
  wrong published); a mirror beside the runtime's would avoid it.
- **Pinned (the GPU agent's review, item 9):** every action by its commit's SHA (the major version
  in a comment); LLVM 21 is Ubuntu 26.04's own clang-21, lld-21 and llvm-21 at 1:21.1.8-6ubuntu1
  (the release pocket, the owner's machine's version; every package of llvm-toolchain-21 held at
  that version by an apt preference), in an `ubuntu:26.04` container, in both
  workflows: no script downloaded and run, and the proxies' SHA-256 printed in the log are a local
  build's. Both workflows declare `contents: read`; only the draft job writes, on a pushed tag. The
  container images stay pinned by tag.

**Licences (the GPU agent's review, items 7 and 8; 2026-10-23).** The LZX decoder
(`tools/xex/lzx_decompress.py` and its C++ port `installer/src/lzx.cpp`) mirrors libmspack's
`lzxd.c` (with its `lzx.h` and `readhuff.h`): the same state (`header_read`, `block_remaining`,
`intel_filesize`, `intel_started`, the length tables with `lzx.h`'s `LENTABLE_SAFETY` 64), the same
frame by frame decoding, `readhuff.h`'s table build, and the Python file's own comments name
`lzxd.c`. So it is taken as derived from libmspack ((C) 2003-2023 Stuart Caie, originally LGPL-2.1):
credited in `installer/third_party/NOTICES.md`, `tools/xex/README.md` and both files' headers, the
licence's text in `installer/third_party/LGPL-2.1.txt`, under GPL-3.0 per its section 3.
The release carries `THIRD_PARTY_NOTICES.md`, `LGPL-2.1.txt`, `LICENSE.txt` and
`APPIMAGE_NOTICES.md` beside the `.exe` (the AppImage carries them inside too), and the AppImage
runtime's libfuse source: `fuse-3.15.0.tar.xz`, `type2-runtime-8f39b89.tar.gz` and the patch alone,
`libfuse-mount.c.diff`.

**The 360's bag remembered (the GPU agent's review, item 4; 2026-10-23).** Uninstall's switch to
PC's bag wrote `bag=pc` alone, so the next uninstall neither warned nor backed up, though a
character not yet loaded and saved with PC's bag still had its items where the 360's put them.
Both installers now write `bag_360_used=1` with the 360's bag and keep it through later installs
and switches; with it and PC's bag, uninstall asks whether each character was loaded and saved
with PC's bag (no by default, `--yes` included; `--anyway` answers yes), then backs the saves up
before uninstalling. Test: `test_the_360_bag_remembered_until_uninstall` (both installers, the
same ini, refusal, backup and folder). No release had the earlier switch, so no ini written
without the flag after a switch is out there.

**No item left out of every bag (the GPU agent's review, item 5; 2026-10-23).** Every move the
pad's menus make (unequip, swap, drop, the pet's bag, buy, sell, the stash, the transmuter, the
enchanter, a gem into a socket) takes the item out with `move_out`, which remembers it and its
place, and ends with `move_done` once it is somewhere. A put PC refuses is answered by
`give_back` (its slot when free, else its bag); the old items of a swap that no longer fit go to
the ground, as PC's swap drops them; one that can be put nowhere is kept (never destroyed) and
stops the game before any save (`move_lost`, the stop of section 21). The menus' pad input runs
in `__try` (`x360_inventory_pad`): an exception with a move under way, or a move left
unfinished, stops the game at once without touching the item (`move_cut_short`): where it went
is not known (it may be placed already, or merged into a stack and freed, 0x4e9743), so giving it
back could duplicate it; no save is written, and loading again has it where it was. A sale now puts the item in the merchant's bag before paying, so an
interrupted sale gives back the item, never the gold with it.

**Saves held while items are aside, and the stop's fallbacks (the GPU agent's review, items 1 to
3, and the owner's decisions after the runs; 2026-10-23).** Every save is refused from the first
item kept aside at load until all are placed (`load_items_aside`), so leaving during the wait for
the pet loses nothing. The save's hook is installed first. Without it the load hook only stops
(`load_cut_only`): at the first item of the player's load that PC would destroy, the item is kept
and the game ends at once, before any save (Torchlight saves on closing, Alt+F4 too: QA run g1-3,
whose save was written four seconds before the game ended). Without the load hook nothing protects
the items: a black screen at the title says so before any character is loaded. A stop whose modal
does not show in 15 s (or that waits two minutes behind another modal) gives its text on the
proxy's black screen (the converted UI's black image, the text laid out as the dialog's paragraphs,
A or Enter; the pad its own until A is let go), and only if that cannot show, `x360_stop.txt` in
Notepad or a Windows box (blank under Proton). `x360_stop.txt` is in `%LOCALAPPDATA%\x360ui`
(the game's folder may be Program Files: no write, or VirtualStore); it is removed once the player
has seen the stop, and one left by a game that ended at once is shown at the next start's title.
More than 64 items kept aside are kept alive and stop the game. PC's loader's puts are logged per
phase: a level change loads the hero's items again, with the load armed (QA run g1-1).

QA runs g1 (the redirected folder `savetest4`, the owner's two save folders' hashes and Steam's
the same before and after each): 1, `test_no_stop`: five saves refused, the save byte for byte;
2b, the normal stop: the 360's modal with the text, A closed the game, the save byte for byte;
2, `test_stop_modal_fails` (before the black screen): the fallback box blank under Proton; 3,
`test_save_hook_fails`: the game ended while the character loaded, the save byte for byte; 3b, the
next start: the stop's text on the black screen at the title, A dismissed it without reaching the
title's menu; 4, `test_save_hook_fails` and `test_load_hook_fails`: the warning on the black screen
at the title, wrapped, before any character was loaded.

**The test options (the GPU agent's review, item 6; 2026-10-23).** `save_root=` (the saves in
another folder, where no installer's backup looks), `hold_saves=1` (every save held back),
`pad_detect=`, `console=` and every `test_*` option are for runs on copies. Both installers drop
them when they write the ini (`install.is_test_option`); the proxy names any it finds at start in
a notice on screen (PC's modal, once in play), so a forgotten one is never silent. Test:
`test_test_options_not_kept` (both installers, the same ini).

**Two games found (the GPU agent's review, a low point; 2026-10-23).** With more than one
supported game and no `--game`, both installers list them; on the console or in windows each is
offered in turn (the first by default) and the one taken is installed or uninstalled; `--yes`
takes the first and prints which, so a run without a console still says where it went;
uninstall offers only the games that have the mod. A question's default is the window's too
(tkinter's `default`, MessageBox's second button, zenity's `--default-cancel`), and zenity shows
texts as they are (`--no-markup`: a path with `&` or `<`). Test:
`test_two_games_shown_and_chosen` (both installers, the same lines). The elevated run passes
`--game` (no second question). The junction risk (the elevated run writes backups into folders the
first run names, which a junction could redirect) stays documented as low: UAC is not a security
boundary, and the folders are the user's own.

**The mod's own files out of the game's folder (2026-10-23).** `x360_ui.log` and
`x360_settings.ini`, like `x360_stop.txt`, are in `%LOCALAPPDATA%\x360ui` (under Wine or
Proton, the prefix's `users/<user>/AppData/Local/x360ui`), through `x360_data_path` in
`cegui_proxy.c`; beside the DLL only when LOCALAPPDATA is missing or its folder cannot be made. The game's folder may be Program Files,
which the game (a 32-bit executable without a manifest) cannot write: Windows redirects its
writes into the user's VirtualStore, where no uninstaller looked. Both uninstallers remove the
`x360ui` folder, and on Windows the three files an older version left in the VirtualStore copy of
the game's folder; with Steam under Proton (the game in `steamapps/common`), the `x360ui` folder in
Steam's own prefix (`compatdata/41500`), which the save backup otherwise leaves alone. `ui_dump.c`'s
developer dumps (`x360_dump_*.json`, only with a `x360_dump.req`) still go beside the game.
`x360_ui.ini` stays beside the game: the installer writes it (elevated when it
must), the proxy only reads it.

**Steam's build first (the owner, 2026-10-23).** Steam's build is no longer experimental: both
installers find and offer it as the others, and with more than one supported install Steam's
comes first (the one `--yes` takes and the first offered; the others in the order found). The
experimental mechanism stays, with no build in it; `--experimental` is still accepted and does
nothing. Earlier entries calling Steam's build experimental predate this. A Steam
install under Proton keeps its saves in Steam's prefix, which the installers leave alone: an
uninstall that would back them up now says none was found and asks (no by default) before going
on without one; `--anyway` goes on and says so. Steam's build has not been played with the mod yet: the README calls it "testing", not
supported, until the owner's run (the repository's rule).

**The release's files (the owner, 2026-10-23).** A release shows the user four files:
`x360ui-install.exe`, `x360ui-install-x86_64.AppImage`, `SHA256SUMS` and
`sources-and-notices.zip` (the proxies on their own, LICENSE, THIRD_PARTY_NOTICES, LGPL-2.1, the
AppImage runtime's notices, libfuse's source and patch, with their own SHA256SUMS inside). The
installer carries the licence and the notices itself: `x360ui-install notices` (miniz's MIT notice
must go with every copy of the executable), embedded by `cmake/embed_blob.cmake`; the AppImage has
them inside too. Requirements: Windows 10 1903 or later, 64-bit (the manifest's UTF-8 code page),
and Linux with glibc 2.35 or later (built on Ubuntu 22.04).

## 25. The installer: Steam from the registry, a window program (2026-10-24)

The owner's first Windows run (Steam and Epic installed) found Epic's copy only: Steam was looked
for in `Program Files (x86)\Steam` and `Program Files\Steam` and the libraries their
`libraryfolders.vdf` lists, so a Steam installed elsewhere was missed. Both installers now read
Steam's own place first: `SteamPath` in `HKCU\Software\Valve\Steam` and `InstallPath` in
`HKLM\SOFTWARE\WOW6432Node\Valve\Steam` (Windows only), then the defaults.

The game's line no longer names its build (`the game: <folder>`; `list` says only "not supported by
this version" for a build it has no proxy for): the build is the installer's business, and Epic's
copy, GOG's build, was shown as "gog".

`x360ui-install.exe` is a window program (`/SUBSYSTEM:WINDOWS`, entry `mainCRTStartup`): a double
click opens no console. Its output goes where it is redirected (a pipe, a file: the tests and
`check_release.ps1`, which pipes it to `Out-Host` so PowerShell waits for it); run from a terminal
without a redirection, it attaches to that terminal's console (`AttachConsole`) and writes there.
The shell does not wait for a window program and keeps reading that console's input, so there
`--console` refuses to ask questions (`set_shared_console`): it needs `--yes` and the options, and
without `--console` the questions are windows. A partial redirection (`2>err.txt` alone) leaves the
other handles as the console's, written without attaching (not checked on Windows yet).


**What the wizard asks first (`install.h`).** The wizard (the owner's design, 2026-10-24) collects
every answer in its window and then runs the same `installer_main` with all of them given
(`--yes --console` and the options), so what it installs is what the command line installs, and
the tests that compare the C++ installer with `install.py` cover it. Before that it asks, without
questions: `games_found` (each install found, Steam's first, with whether its build is supported,
whether it has the mod, its bag and whether the 360's bag was ever used), `game_in` (a folder or its
`Torchlight.exe` the player chose), `package_problem` (the 360 copy's header and the files it must
hold, without extracting or decrypting), `saves_to_back_up` and `needs_permission`. The tests reach
them through `dev games`, `dev game-in`, `dev package-check` and `dev saves`. The install reports
its progress by phases (reading the 360 copy, the backup, each of the six conversions, putting it
in place) through `set_progress`; an elevated run writes it to `--progress-file` (one line,
`fraction<TAB>text`, replaced whole each time), which the first run reads while it waits.

## 26. The installer's window: the wizard (2026-10-24)

The owner's design after the first Windows run ("one real window, a wizard"): a double click (no
arguments) opens one window, the same on Windows and Linux, played with a pad, the mouse or the
keyboard; with arguments the executable is the command line it was.

**What it is made of.** SDL 3.4.14 and Dear ImGui 1.92.5 (the versions of the ReXGlue SDK the
recomp uses), built static from release archives fetched and checked by hash
(`installer/cmake/wizard_deps.cmake`; `X360UI_DEPS_DIR` for offline builds); ImGui draws through
SDL_Renderer (Direct3D 11 on Windows; OpenGL, Vulkan or software on Linux), so no graphics library
is loaded by hand. Noto Sans 2.015 (regular and bold, OFL) is built in, at 19 points times the
display's scale (`SDL_GetDisplayContentScale`; ImGui's `ScaleAllSizes` and `FontScaleDpi`, as its
SDL3 example), so it is sharp on high-density screens. The palette is the ReXGlue SDK's ImGui one
(the recomp's look). On Linux SDL loads X11, Wayland, D-Bus and udev at run time: the executable
still needs only libc and libm. A display of 1280x800 or less (the Steam Deck) gets the window
full screen.

**The screens** (`installer/src/wizard/screens.cpp`): the games found (`games_found`, Steam's
first, each with its store, folder and whether it has the mod and which bag; "Look for another
folder..."), the 360's package (checked at once by `package_problem`), the bag (the 360's warning
in a box on the same screen, with the saves it backs up, or the choice to go on without a backup
when none is found), the summary (what, where, the backup, whether Windows will ask for an
administrator), the progress (`set_progress`) and the result (the backups, "Open the game": Steam's
`steam://rungameid/41500`, else the executable on Windows). A game with the mod opens "What to do":
reinstall, change the bag (to PC's: only the setting, as the command line's first uninstall step;
to the 360's: the package and a new conversion) or uninstall, where the 360's bag's two steps are
explained and offered (switch to PC's bag; or, once each character was loaded and saved, the
uninstall, the same answers as `--anyway`). Each screen ends in Back and its next step; B or
Escape goes back, Start is the next step, A or Enter chooses.

**How it installs.** The wizard runs `installer_main` on a thread with every answer given
(`install --game G --package P --bag B --yes --console`, `uninstall --game G --yes --console
[--anyway]`), its output kept for the result's details. UAC is asked only then, by the command
line's own `with_permission`, whose elevated run reports its progress through `--progress-file`.

**The pickers.** SDL's (Windows' own; on Linux the desktop portal only, never zenity). Where there is
none (no portal: the Steam Deck's Game Mode) the wizard's own browser opens: the folders and files
of a folder, the folder above, Windows' drives, "Use this folder", all played with the pad.

**The command line** asks on the console only (the windows of zenity, kdialog and `MessageBox`
are gone: the wizard is where questions are answered in windows). From a terminal on Windows,
whose input the shell shares with a window program, an install or uninstall needs `--yes`.

**Tests** (`tests/test_installer_cpp.py`), with SDL's offscreen video and software renderer and a
simulated pad (an SDL virtual gamepad, given to ImGui alone: a real pad on the machine would
otherwise be the one ImGui reads): `test_the_wizard_end_to_end` installs the 360's bag through the
package chosen in the own browser and compares the folder with `install.py`'s byte for byte, then
switches to PC's bag and uninstalls through the two steps, the folder as before;
`test_the_wizards_screenshots` reaches every screen and, with `X360UI_SHOTS`, keeps their PNGs at
960x600, 1280x800 and scale 2 (CI uploads them: Linux offscreen, Windows in its own window and
renderer). The scripts (`dev wizard --script`) are lines: `screen NAME [SECONDS]`, `press BUTTON`,
`down`/`up BUTTON`, `key KEY`, `text TEXT`, `wait N`, `shot NAME`, `shot-if SCREEN NAME`,
`debug`, `quit`. ImGui ignores a first A while its pad cursor is hidden, so each screen's default
item is focused with the cursor shown.

**The administrator's permission, from the wizard.** The wizard never asks for it when it opens: the
summary says "Windows asks for an administrator's permission when you start" (`needs_permission`),
and only Install, Switch bag or Uninstall makes the command line's `with_permission` run the
installer again through UAC with every answer, while the window shows that run's progress (its
`--progress-file`) and then its result (its `--result-file`). Windows' CI has no UAC to answer, so
`test_the_wizard_through_the_elevated_run` takes that way with two test switches:
`X360UI_TEST_NEEDS_PERMISSION` (the folder taken as needing it) and `X360UI_TEST_ELEVATE_AS_USER`
(the second run started as the same user, without UAC's prompt). The prompt itself is the owner's
Windows test.

**PC's tip never drawn (2026-10-24).** The owner saw PC's tip window for a moment before the 360's
took its place: `tip_tick` looked for it every 250 ms and loaded the 360's layout the first time it
saw it, so PC's showed until then. PC loads `tipmenu.layout` once, at the game's start; its root
(`Frame`) is now made transparent there, in `x360_pc_layout_loaded` as the HUDs are, and kept so
`tip_tick` finds the window at once. It is shown again only when its text is none of PC's tips (the
settings' confirmations, used with the mouse), and made transparent again each time it closes. Only
`tip_tick` shows it again, so it is made transparent at load only when the pad's frame runs (which
runs the tick: `x360_pad_running`), and shown again for good if the 360's tip layout cannot load.
The inventory's tip (TIP_INVENTORY, asked for as the 360's inventory opens) showed only once the
inventory closed. Not its order (the first try: AlwaysOnTop and moved to the front each frame did
not change it): the 360's screens hide PC's whole sheets (`hide_pc_ui`: Ingame UI Sheet, Top UI
Sheet), and PC attaches its tip window to the Top UI Sheet only when it shows a tip, so the tip went
into a hidden sheet and `tip_tick`, which asked for the window's visibility with its parents', saw
none until the sheets came back. The second try (hiding the tip's sheet window by window) failed
for the same reason: when the inventory opens the tip is not attached yet. What works (the owner's
run, 2026-10-24): `pc_tip_up` asks only the tip's own window (attached, and shown itself), and
`press_tip` shows its sheet for the click on PC's Ok only, in front of the inventory, then hides it
again. The 360's tip stays AlwaysOnTop and in front each frame. On the 360
the tip keeps every button it gets (sub_823589A0 returns 1), so while one shows (and 0.3 s after
it closes) the inventory gets no buttons, nor do Back (the inventory) and Start (the pause):
`x360_tip_open`, read by `proxy/pad.c`.

**The hero not played during a cinematic (2026-10-24).** The owner could move the hero with the pad
during a cinematic, which the 360 does not allow. The 360's frame update (`sub_821EBA48`) calls
`sub_821ED810` and, while it is true, skips the block that turns the pad into play; its
setDestination (`sub_8228F8C8`) asks it too. PC has the same test, `0x541db0` (the same chain of
fields: CGameUI's `+0x1074`, its `+0x18`/`+0x14`, `+0x2c`, a byte at `+0x1018` where the 360 reads
`+4140`, ...), a thiscall on CGameUI, which the client keeps at `+0x3c` (PC's client update calls it
so at `0x415096`; Steam's at `0x414e76`, the function at `0x53d830`, verified by
`port_addresses.py`). PC's setDestination asks it too (`0x494d2e`), but the proxy also turns,
attacks and holds the hero by other calls, so `proxy/pad.c` now returns before the world's input
while it is true, after the menus, tips and dialogs (which keep their pad). A first try passed the
hero as `this`: the game crashed entering play (`0x541dba`). The owner's run confirmed it.
What the 360 does while it holds (`sub_821EBA48` after its second call, `r3` the client's `+60`):
it clears the client's input state (`+3964`, `+3936`..`+3941`, `+84`..`+87`), and if the hero is
in a skill (`sub_821DC600`) or the like sets its mode to 2 (its virtual `+368`), then returns from
the frame. PC's client input (`0x415125`) has the same branch: it clears its input state
(`0x4e7350` on `+0xf4c`, `+0x54`..`+0x56`), and if the hero performs a skill (`0x486550` or
`0x486390`) calls its virtual `+0x174` with 2, then returns. PC runs it every frame by itself, so
the pad has only to give no orders while it holds.

## 27. The pad against the 360, function by function (an audit, 2026-10-24)

The owner asked for the pad's play to be checked against the 360's code as a whole, not piece by
piece as it was written. The 360's frame update (`sub_821EBA48`) read in order, against
`proxy/pad.c`:

| What | The 360 | The pad | |
|---|---|---|---|
| A cinematic | `sub_821ED810` on the client's `+60`: input cleared, a hero in a skill set idle, the frame left | no orders (PC's own `0x415125` does the rest) | same (section 26) |
| Zoom | RS, `-0.35` a frame, from 14 to 28.5 times the level's factor | the same rule, by the clock | same |
| Facing | toward the stick's point every frame; with no stick and the flag, the facing kept (`sub_8221C838`) | `setTargetDirection` every frame with the stick | same |
| Walking | `setDestination` 2 units ahead every 0.1 s | the same; `stopPathing` on release | same (the release measured, section 9) |
| A skill's lock | `sub_821DC600` (the skill at `+708`) | `skill_locks_movement` (GOG `+0xc`) | same |
| X | `sub_8221BC80(-1)`: no walking, the attack where the hero stands | PC's attack in range, else in place toward the target | same (section 9) |
| A dying or dead hero | no target, no moves, no buttons (`0x821EC0E8` skips to `0x821EC9F4`, which drops the candidates' marks at the client's `+444`, `+452`) when its AI state (`+632`, GOG `+0x284`) is 5 or 6 (*(OT)* `AISTATE_DYING`, `AISTATE_DEAD`) or it is not alive | **was:** the search and the highlight went on | fixed on the branch `audit-dead-hero` |
| The target's choice | `sub_821EB598` every frame (alive, not 5 or 6) | every 15 ms | same rule |
| B, Y, LT, RT | `sub_8221BC80(skill)`, aimed by `sub_8221C838` | PC's hotbar keys 3 to 6, the virtual cursor 2 units ahead | to compare: the aim and the target a skill takes |
| LB, RB | not read yet | PC's hotbar keys 1 and 2 | to read |
| Walking, read again | `sub_82199E08(client, 0)`, every frame while the hero is not aiming (`hero[510]` 0) and its AI state is not 34: past the stick's 0.1 dead zone, a destination **5.0** units ahead (the stick turned by the camera) **every frame**, and `hero+1500 = min(len² + 0.2, 1.0)` first (an analog speed?); at rest, `+1500` back to 1.0 and, within 2.0 of its goal, the hero faced 2.4 units along the stick (`sub_82199360`). `sub_8221B8E8` (2.0 units on a 0.1 s timer, section 9) runs only after an action held (`sub_8221BC80` returning 0) | 2.0 units every 0.1 s, a fixed speed, `stopPathing` on release | **different, not changed yet**: `+1500` is read by `sub_821D5410` and `sub_822E3A30`, not yet read; PC's `+0x5e8` (`+1500 + 0xc`) is written by its path code (`0x48cd51`), so likely another field. To read before any change (it may be the "remolino" of section 9) |
| Start and Back in a cinematic | not traced | handled before the cinematic's test | to trace |
| Paths and collisions | the engine (A*, `passableBetween`) | PC's engine | to confirm with OpenTorchlight |

## 28. PC's menu keys swallowed in play (2026-10-24)

The owner's first Windows run: the space bar (PC's KEYMAP_CLOSEALL) made PC's menus move behind
the 360's. With the 360's interface the pad opens the screens, so in play the mod keeps PC's menu
keys from the game, by the owner's list: KEYMAP_INVENTORY (I), PET (P), STATS (C), SKILLS (S),
QUESTS (Q), JOURNAL (J), OPTIONS (Esc), PAUSE (Enter), CLOSEALL (the space bar) and AUTOMAP (A:
the map is on R3, section 9). The rest work as on PC: the hotbar's 1 to 0 (the pad sends them for
the skills), F1 to F12, Shift, Alt, Tab, W, the skills' cycle (`.` `,`), the zooms (`=` `-`,
`]` `[`) and the console's key. A dual mode (the keyboard's menus as PC's) may come later.

How: the game's window is subclassed once (`keys_off`, `proxy/x360_menu.c`); its procedure drops
WM_KEYDOWN and WM_KEYUP of those keys, and WM_CHAR of Esc, Enter and space (CEGUI's injected
characters), while in play (`hud_in_play`), PC's settings are not open (reached from the 360's
pause menu; the keyboard works there), the developer console is not shown (its CEGUI window
`WindowsLook/Console`, typed into) and no cinematic holds (CGameUI's `0x541db0`: PC's keys skip
it). A press's repeats and release go as the press went, so PC never keeps a key down across those
changes; out of play, or when the frame tick has not refreshed the state for 2 s, nothing is
swallowed. The console's window is looked for at once when its key (`` ` ``) goes down.
The game's window (its title) is subclassed once, with its own kind (ANSI or Unicode). Test scripts that send these keys in play (`tools/send_input.py`) now send
nothing the game sees. PC's window procedure (`CGame::WndProc` *(OT)*, in OpenTorchlight's
build; not located in GOG's) is what gives the keys to `CGameUI::keyEvent` and
`CKeyManager::keyEvent` *(OT)*, so neither sees them. That GOG's keys come by the window's
messages is seen in every run: the pad's skills are `WM_KEYDOWN` messages it posts (`post_key`,
`proxy/pad.c`), which PC's hotbar acts on. The
one address used is the cinematic's test, already ported to Steam's build
(`proxy/addresses_steam.txt`). The six screen
hotkeys patched off before (`hotkeys_off`, section 17) stay off. At the title and the main menu
the keyboard is PC's.

## 29. The stick's speed factor (the movement plan's change a, 2026-10-24)

The 360's plain stick walk (`sub_82199E08`) writes `hero+1500 = min(len^2 + 0.2, 1)`, and its
character update (`sub_821D5410`, `0x821D6ED0`..`0x821D6F10`) multiplies the speed's scale by it;
GOG's same update (`0x4a6930`) has no such block (the audit's analysis, PR #30). The owner's
recording replayed on the recompiled 360 build (its agent's trace, the same input) confirmed it: a
straight at a 0.72 tilt walked at 4.36 units/s there, 6.22 on PC (the formula's 0.72 gives 4.47),
and the half-tilt spins at 2.4 units/s there, 5.3 and 4.1 on PC.

The factor is now in GOG's character update (`proxy/pad.c`, `walk_scale`): a jump at its join
(`0x4a8062`; Steam's `0x4a5dd8`) to a trampoline that scales the speed's stack slot (read from the
store just before: GOG's `[esp+0x1c]`, Steam's `[esp+0x18]`) by `min(len^2 + 0.2, 1)`, for the
hero only, unless its AI state is 7 or 9 or the 360's `sub_821DC600` holds (inlined in GOG's
`setDestination`, `0x494b67`: the skill's `+0xb4`, `+0x3c`, the hero's `+0x20b`, `+0x2c0`) with
the skill's byte `+0x40` set. The factor is computed each frame while the hero is not aiming
(`+0x20a`) and its AI state is not 34, and kept otherwise, as the 360 keeps `+1500`; from the
stick only while the pad's walk takes it (its dead zone, until change c gives it the 360's 0.1),
else 1. `pad_walk_factor=0` leaves it out, to compare. It changes what the player sees (slower at
a partial tilt): merged only with the owner's OK after a test by hand.

**Measured** (the owner's route replayed on GOG with the factor; units per second over each route):

| Route | PC before | With the factor | The 360 |
|---|---|---|---|
| a spin at 2.0 turns/s, half tilt | 4.10 | 2.33 | 2.35 |
| a spin at 1.5 turns/s, 0.85 tilt | 3.50 | 3.32 | 3.39 |
| a spin at 0.66 turns/s, half tilt | 5.35 | 3.17 | 2.41 |
| a straight at 0.72 tilt | 6.22 | 5.09 | 4.36 |
| a straight at 0.94 tilt (factor 1) | 6.22 | 6.22 | 5.37 |

The replay reproduces the recording (the 0.94 straight: 32.20 units against the recording's
32.22). What stays apart is the path's shape (the 360 aims each frame: change b, parked, PR #40).
## 30. A destination each frame (the movement plan's change b, 2026-10-24)

(Parked here at first; measured again on top of the 360's dead zone in 30a, which supersedes the
measurement below.)

The 360's plain stick walk (`sub_82199E08`) sends a destination each frame, at 5 times the stick's
length along the stick turned by the camera; the pad sent one every 0.1 s, 2 units ahead
(the 360's walk after a held action, `sub_8221B8E8`). The owner's route replayed: PC's paths are
polygons of 0.1 s chords where the 360's are curves (the spins). Now `setDestination` goes once
per game frame (the proxy's frame listener counts them, `x360_frames`) at `5 * len` along the
stick's world direction (the camera's yaw since section 31, refreshed on a turn of more than 5
degrees or every 0.1 s, by its own timer); the facing each frame and the release (`stopPathing`) are unchanged.
Left out: the 360's `sub_8219C070(hero, 0)` and its `+652` handle's release each frame after the
destination (the pad drops the target on the stick's first frame). The 360 runs it at about 60
frames a second, PC at its uncapped rate (100 to 200): twice to four times the path requests;
measured in the replay (the frame rate in the trace). `pad_walk_dest=0` keeps the old one, to
compare.

**Measured (2026-10-24, the owner's route replayed; units per second over each route).** Change
(a) alone brought the partial-tilt spins to the 360's (2.0 turns a second at half tilt: 2.33,
the 360's 2.35; 1.5 at 0.85: 3.32 against 3.39). With (b) on top they moved away again (3.21 and
3.61; 0.66 turns a second at half tilt: 3.63, the 360's 2.41, (a) alone 3.17); the straights did
not change. Two causes were tried and left out:
- the destination each frame itself was the change being measured;
- the screen probe's direction against the 360's camera yaw: the angle between the stick and the
  walk varies the same way on both builds (about 20 degrees across the stick's directions), with
  a constant offset of about 6 degrees between them, so the probe does not bend it (wrong, as
  the 8-direction replay at full tilt showed: section 31).
Stopped there (two causes in a row): (b) is parked, (a) stands alone. Not tried yet: the frame
rate (the 360's replay ran at about 177 frames a second, PC's at 105 to 120: a destination each
frame is then a different rate), and the 360's per-frame `sub_8219C070`, left out. Both were
tried later and changed nothing (a replay each); then the 360's stick dead zone was found
(section 32), which changes the factor's input, and (b) was measured again on top of it (30a).


### 30a. Change b again, on top of the 360's dead zone (2026-10-24, the owner's new route)

With changes a, the yaw and c integrated, the owner's new route (section 32) left one difference:
the fastest spins (the stick turning 650 to 720 degrees a second at full tilt), where PC walked 3.4
to 4.8 units a second against the 360's 5.2 to 5.5. Change b (a destination each frame) replayed
on the same route, against the 360 and develop's replay:

| | 360 | develop | develop + b |
|---|---|---|---|
| fast spins, 27.75 / 28.0 / 28.25 s (units a second) | 5.51 / 5.35 / 5.22 | 3.87 / 3.43 / 3.90 | 4.99 / 5.32 / 5.21 |
| fast spins, 51.0 / 51.25 s | 5.42 / 5.31 | 4.78 / 4.32 | 5.11 / 5.33 |
| path, 27 to 29 s and 51 to 53 s | 9.20, 9.90 | 8.05, 9.44 | 9.14, 9.95 |
| start | 860 ms | 860 ms | 861 ms |
| whole path | 193.6 | 191.1 | 193.2 |
| gap to the 360, every 0.1 s: largest, mean | | 1.34, 0.35 | 0.96, 0.30 |
| speed at a steady tilt of 0.75 / 0.80 / 0.85 | 3.16 / 3.71 / 4.54 | 3.27 / 3.67 / 4.43 | 3.17 / 3.68 / 4.53 |

At 0.6, 0.7, 0.9 and full tilt the three agree within 0.03. So b brings the fast spins to the
360's and makes nothing else worse; its earlier result (section 30) came before the dead zone,
when the factor read the raw stick. The 0.1 s timer stays as a backstop (a frame listener that
stopped would else hold the hero), and the call's cost goes to the log every 5 s while the stick
walks with `pad_log=1` or a trace on (`pad: setDestination us avg ... max ... n ...`): a path
request each frame, against
walls too, to be read in a run before it is integrated.

**Its cost (2026-10-24, the owner's request).** The owner's new route with the fixed b: the gap to
the 360 0.73 largest and 0.21 mean, the fast spins 5.06 to 5.44, the start 868 ms. Against walls
(the first 8-direction file, 2 s pushes from the boxed-in town start), with b and with
`pad_walk_dest=0`, at 60 fps with vsync (`VSYNCH :1`, `MAX_FPS :60` in the measuring copy's
`local_settings.txt`) and uncapped (`VSYNCH :0`, `MAX_FPS :1000`); each frame's time and the
`setDestination` time in it from `x360_frames-<date>.csv` (pad_trace), 6 to 30 s:

| | 60, b | 60, no b | uncapped, b | uncapped, no b |
|---|---|---|---|---|
| frames a second | 60 | 60 | 172 | 182 |
| frame ms p50 / p95 / p99 | 16.64 / 17.29 / 17.55 | 16.66 / 17.42 / 17.84 | 5.19 / 8.58 / 9.20 | 4.60 / 9.55 / 10.48 |
| largest frame | 84 ms | 100 ms | 64 ms | 52 ms |
| frames over 18 ms | 7 | 10 | 1 | 1 |
| of them with `setDestination` over 1 ms | 0 | 0 | 0 | 0 |
| `setDestination`: calls, mean, largest | 954, 30 us, 71 us | 160, 23 us, 48 us | 2775, 41 us, 386 us | 160, 28 us, 363 us |

The long frames (52 to 100 ms) are on both sides, with `setDestination` at 30 us at most in
them: the game's. At 60 fps b makes no frame miss its 16.6 ms; uncapped it costs 0.6 ms of the
median frame. (On the route at 200 fps three 5 s windows had a call of 3.3 to 3.7 ms; none here.)

## 31. The walk's direction: the stick turned by the camera's yaw (2026-10-24)

The 360's plain walk (`sub_82199E08`) turns the stick by the camera's yaw: `f10 = atan2` of the
camera's forward (`sub_821C9BF8`, the camera controller `client[16]`, its camera at `+8`, the
orientation at `+0x410`, times `-UNIT_Z` at `0x8349D4EC`; `sub_821BEBE8` the atan2), then
`f9 = f10 * 57.2958 - 90` (the pool `0x820AA2C0`'s `+0x27C` and `+0xB4`), the rotation's angle.
On the ground that is: stick up walks along the camera's forward `(fx, fz)`, stick right along
`(-fz, fx)`. The pad took the direction from a screen probe instead (a point beside the hero on
the screen, cast to the ground by `findWorldLocation`), which perspective bends.

PC's camera is the 360's: Ogre's `Camera::getDerivedDirection` and `getDerivedPosition` of the
controller's camera (client `+0x10`, then `+8`) read (-0.485, -0.728, 0.485) and (-19.584, 18.896,
4.756) at the town's start; the recomp agent read the 360's (-0.48507, -0.72761, 0.48507) and
(-19.583, 18.896, 4.755). Now the pad turns the stick by the yaw of that direction, read each
time the walk's direction is refreshed (a turn of more than 5 degrees, or every 0.1 s); the
screen probe stays as the fallback when the camera cannot be read, and `pad_walk_yaw=0` in
`x360_ui.ini` keeps it, to compare.

**Measured (2026-10-24).** A synthetic replay (outside the repository): a lead-in to an open
place in town, then full tilt at once in 8 directions, 0.6 s each (full tilt, so the stick's
length plays no part), on the recompiled 360 and on GOG with each direction. The walk's angle in
the world (atan2 of z, x) from 0.15 to 0.55 s after the press, every push at a steady 5.8 to 6.7
units a second:

| Stick | 360 | PC, screen probe | PC, camera yaw |
|---|---|---|---|
| up | 139.3 | 128.7 (-10.6) | 141.8 (+2.4) |
| down | -33.1 | -41.3 (-8.2) | -30.0 (+3.2) |
| right | -135.0 | -142.0 (-7.0) | -135.0 (0.0) |
| left | 45.0 | 52.0 (+7.0) | 45.0 (0.0) |
| up-right | -179.9 | 163.2 (-16.9) | -180.0 (0.0) |
| down-left | 1.6 | -4.5 (-6.1) | 2.0 (+0.4) |
| up-left | 84.7 | 103.2 (+18.5) | 85.2 (+0.5) |
| down-right | -86.0 | -90.2 (-4.2) | -86.1 (0.0) |

Up and down leave the exact diagonal on both builds there (the ground's). On a reversal the
facing turns 150 degrees in 196 ms on the 360, 197 to 202 with the yaw, 224 to 249 with the
probe. Not yet played by hand.

## 32. The sticks' dead zone, the 360's (the movement plan's change c, 2026-10-24)

The recompiled 360's walk starts only past a stick length of about 0.48, and below full tilt its
speed is lower than `l^2 + 0.2` gives on the raw stick. The recomp agent's probe found why (the
owner approved it; branch `diag/stick-read` a623c49 of the recomp): at `XamInputGetState`'s return
the stick equals the one given in every frame (17300 frames), and at `sub_82191018`'s return it
is rescaled. Between the two is the game's per-tick input update, `sub_821FE1D0`: when the poll
flag `0x83065EAA` is set (by `sub_821EF318` at `0x821EF79C`) it polls (`sub_821FE8A8` copies
each pad's `XINPUT_STATE` into `0x83557E80 + 16 * user` and clears the flag); then, for the active
pad, it rewrites the sticks in that table in place, `sub_821FE778(&lx, &ly, 0x1EA9 = 7849)` and
`sub_821FE778(&rx, &ry, 0x21F1 = 8689)`, and the triggers with `sub_821FE480`. `sub_821FE778` is a
radial dead zone in shorts: the length less the dead zone, at least 0 (the pool's `+0x44`), times
`32767 / (32767 - dz)` (`+0xD4`), along the stick's direction (divided by the length when it is
over `+0x280`, 1e-8), clamped to 32767 (`+0x2A8`, `+0xD4`), truncated (`fctiwz`). The update has
three callers, `sub_821EBA48` (the client's frame update), `sub_8221ABA0` and `sub_82338218`; only
the first after the flag polls, and a later one in the same tick runs the dead zone again on the
table already done. So the stick goes through it twice: zero up to `2d - d^2` = 0.4217 (`d` =
7849 / 32767), and `f(f(l))` above, `f(l) = (l - d) / (1 - d)`. The probe measured 0.4218, and
0.503 -> 0.140, 0.703 -> 0.486, 0.900 -> 0.827 (11137 frames, within 0.001 of the formula). The
walk's own 0.1 (`sub_82199E08`) applies after it: the hero walks from 0.48. The recomp agent's
caveat: the two passes come from the same game code's call order, measured on every partial-tilt
frame, but the calls per tick were not counted.

Here `poll()` passes the left stick through `sub_821FE778`'s rule twice with 7849 and the right
stick twice with 8689, whatever the source (XInput, a replay, the pad file), and the pad's
threshold becomes the walk's 0.1 on the result (it was XInput's 0.24 on the raw stick). The
walk, its speed factor (`l^2 + 0.2`, section 29), the skills' aim and the zoom read the result;
the proxy's own menus (the pause menu, the inventory, the navigation of PC's screens) keep the
raw stick (their 0.5 is not the 360's code), and the recording and the trace keep the raw stick.
It is computed with the stick normalised (the proxy's square root converges near 1, not on shorts
squared: the review), the shorts' truncation kept. The triggers are left as they were (PC's 64 of 255;
`sub_821FE480` not read). `pad_dead_zone=0` in `x360_ui.ini` keeps the old behaviour, to compare.

**Measured (2026-10-24, the owner's route replayed with changes a, the camera's yaw and this; the
360's trace with the recomp agent's probe).** The starts are the 360's: route 0 starts walking
1780 ms after the stick leaves rest (the 360: 1790; before this change 57), route 3 729 (739;
144), route 4 121 (116; 54); route 0's path is 27.50 units (27.48). By the stick's length, the
speed and the scale (GOG's `+0x200`, the 360's `+500`) over 0.1 s with the stick steady:

| Stick | 360 speed, scale | PC speed, scale |
|---|---|---|
| 0.50 | 1.49, 0.23 | 1.37, 0.22 |
| 0.65 | 2.21, 0.35 | 2.13, 0.35 |
| 0.70 | 2.94, 0.46 | 2.91, 0.47 |
| 0.80 | 4.24, 0.68 | 4.20, 0.68 |
| 0.90 | 5.65, 0.85 | 5.50, 0.85 |
| 1.00 | 6.24, 1.00 | 6.24, 1.00 |

In route 1 the PC hero was held at full tilt by something in its way, at about (-17.8, 28.2),
in both replays with this change (1.7 s, then twice for about 0.5 s; the 360 passes there, and
PC's replay before this change walked another line, its route 0 ending 4 units away): an
obstacle on PC's path there, the pet or a townsman. So the later routes start from other
places (up to 4 units), and their stop-and-go stretches (routes 3, 4, 6, the stick stepping)
differ by 6 to 35 % in total path while the speed by length above agrees and most 0.5 s samples
match within 0.2 units a second. Not played by hand yet.

**A new route (2026-10-24), recorded by the owner with changes a, the yaw and this, touching
nothing (`route-new.txt`, 55 s, outside the repository).** Replayed on the 360 (the recomp agent;
its probe: the raw stick at `XamInputGetState` in every frame, the game's read zero up to 0.4204
and more from 0.4239) and on PC, and against the owner's own run: the paths are 193.6 units (the
360), 191.1 (PC replayed) and 190.4 (PC by hand); the heroes stay within 0.95 units of each other
the whole route (the end 0.65 apart); the start is 863 ms on all three; most 2 s windows agree
within 1 %. The one difference left is the fastest spins: with the stick turning about 650 to
720 degrees a second at full tilt, PC walks 3.4 to 4.3 units a second where the 360 walks 5.2 to
5.5 (27.75 to 28.25 s, 51.0 to 51.25 s), 5 to 12 % less over those 2 s windows; at 430 to 600
degrees a second the speeds agree. The pad's destination goes every 0.1 s, the 360's every frame
(change b, parked in section 30 before the dead zone was found).

## 34. PC's resurrection screen with the pad (2026-10-24)

In the owner's run of 2026-10-24 the hero died and PC's resurrection screen (`diemenu.layout`:
`Frame` holding `Title`, `Resurrect1`-`3` with their `Choice1`-`3` texts, and `ExitGame`) could
not be left with the pad: no code gave its buttons to the pad. The 360 has its own screen (its
`diemenu.layout`, converted with the others) that the port does not show yet; the owner chose the
quicker way for now: PC's screen with the pad, as PC's pause menu (section 17's list of PC's
buttons).

- `pc_menu_tick` finds its prefix by `Resurrect1` (and `Resurrect2` under the same prefix), and
  while its `ExitGame` is attached and shown in play, with no screen of the 360's over it (pause,
  inventory, tip, modal, dialog), collects the buttons under its `Frame` only (PC's HUD buttons
  are on the same sheet, transparent).
- The d-pad or the stick moves between them (top to bottom), A clicks the focused one. The play
  pad gets nothing meanwhile (`x360_pc_menu_up`, as PC's settings: no move, skill, Start or
  Back). The log names them as `PC's buttons for the pad 4 (resurrection)`.
- Not ported: the 360's own resurrection screen (its look and its pad, which a port would read in
  the 360's code first).
## 35. Start during a cinematic (2026-10-24)

The owner saw the pause misbehave during a cinematic. **[read]** The 360's frame update
(`sub_821EBA48`, `r31` the client, CGameUI at its `+60`) handles Start (the client's `+612` set
to 1: Start by its behaviour, its writer not traced **[inferred]**) only when CGameUI's menu at
`+968` is not visible, the level is up (the client's `+44`, its virtual `+32`) and `sub_821ED810`
(the cinematic: "The hero not played during a cinematic", above) is false. Then, on CGameUI: with
its `+6161` set, `sub_8233EEF8` clears it and closes the menus; else, unless a dialog at `+5312`
holds it, any open menu is closed (`sub_821E4B78`, then `sub_8233E998`); else, unless the menu at
`+956` is visible, the menus are closed (`sub_8233E998`) and the pause toggled (`sub_8233D440`,
the menu at `+932`, `COptionsMenu`). During a cinematic Start does nothing.

**[seen]** The owner on the 360 (2026-10-24): during a cinematic Start skips it and Back opens
nothing; at an NPC's screen (a merchant, a stash) Start and Back both close it. The skip is not in
the frame update read above (its handler is not traced; the cinematic menu's own pad is likely).
Back there: the 360's `+705` toggles CGameUI's `+1000` after the cinematic's first test, not
traced further.

`proxy/pad.c` opened the 360's pause on Start during a cinematic. Now, while PC's `0x541db0` holds
and no pause or inventory is open, Start clicks PC's cinematic menu's `Skip`
(`x360_cinematic_skip`: cinematicmenu.layout's `Skip`, found by its `DialogTextFrame`, a left
click at its centre) and Back is dropped; not while a tip is up (it keeps every button). In the 360's inventory at a shop, Start closes the shop as
Back and B do (`x360_inventory_pad`); elsewhere in the inventory it keeps the pet's commands.

**[seen]** The owner on the 360: in a quest's talk Start and Back do nothing; a common talk is
closed by them; a merchant's too; an in-play cinematic is skipped by Start, which also accepts
its quest. Now Start and Back do nothing in any talk (`proxy/pad.c` keeps them from the pause and
the inventory while a talk or another of the 360's overlays is up): right for a quest's talk; a
common talk is not closed by them, because its pages look the same as a quest's first ones (Ok
only) and the port has no sign yet of which it is (the quest marker is taken off without reading
which it was).

**Not working (the owner's run of 2026-10-24, late; stopped after two failed tries):** in play,
after entering the mine, `0x541db0` held for a while with no talk shown and no Skip attached:
twelve Starts logged `PC's Skip not shown` and nothing was skipped. So PC's in-play cinematics
are not skipped through cinematicmenu.layout's Skip; how PC skips them (a key of its own, its
CCinematicMenu's input) is not read. **[seen]** The owner: PC's Escape does there what the 360's
Start does. `x360_cinematic_skip` now posts Escape (key down and up, as `pad.c`'s `post_key`) to the game's window when
no Skip is attached; `keys_proc` lets it through while the cinematic holds.
