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
| The proxy, installed | the game folder: `CEGUIBase.dll` (proxy), `CEGUIBase_orig.dll` (real), `x360_ui.ini`, `x360_ui.log` |
| Steam snapshot (hashes) | `~/torchlight-gog/hashes/steam-guard.json` (2404 entries, 2026-10-15) |
| Steam's Torchlight install (read only, for comparisons) | `~/.steam/debian-installation/steamapps/common/Torchlight` |
| The Xbox game data (read only) | `~/360tools/extracted/extracted` (`pak.zip`) |
| The first prefix (wine alone; it does not work, now only holds the game folder) | `~/torchlight-gog/prefix` |

**Running GOG, step by step:**
1. Snapshot Steam:
   `tools/steam_guard.py snapshot ~/torchlight-gog/hashes/steam-guard.json ~/.steam/debian-installation/steamapps/compatdata/41500 ~/.steam/debian-installation/userdata/<steam-id>/41500`.
   If a snapshot was already taken, check that it still holds with `verify` before overwriting it.
2. Run `tools/run_gog.sh`. It calls `proton run` from Proton Experimental with:
   - `STEAM_COMPAT_DATA_PATH=~/torchlight-gog/compat`;
   - `STEAM_COMPAT_CLIENT_INSTALL_PATH` set to Steam's folder, only so the script finds it;
   - `SteamGameId=gogtl` and `SteamAppId=0`, both needed: without them the script exits at once
     with code 1 and no log;
   - `WINEDLLOVERRIDES="winemenubuilder.exe=d"`, so no shortcuts are made;
   - `PROTON_LOG=1`.

   Steam need not be open: GOG's executable does not import `steam_api`, so "Failed to connect to
   Steam" in the log is harmless.
   With the proxy installed (section 7), `x360_ui.log` in the game folder lists every scheme and
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
| `compare_ui.py` | A 360 catalog screen against PC's dump of the same screen, window by window (section 14) |
| `layout_windows.py` | The named windows of a CEGUI layout, and how many of them another layout has |
| `convert_x360_ui.py` | Stage 0: the 360 UI files from the user's `pak.zip` into `OUT/media/ui/x360/` (section 6) |
| `build_proxy.py` | Stage 1: builds the proxy `CEGUIBase.dll` from the user's DLL (clang-cl, lld-link); installs and removes it in a game folder |
| `grab_window.py` | A screenshot of the game's window (X11 under Xwayland) |
| `send_input.py` | Mouse and keyboard input for the game, sent from inside its prefix (`send_input.exe`, built by `build_proxy.py`) |
| `steam_guard.py` | Hash snapshot and verify of folders that must not change |
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

**The DLL.** `proxy/cegui_proxy.c`, built by `tools/build_proxy.py build REAL_DLL BUILD_DIR` with
`clang-cl` and `lld-link` 21 (`--target=i686-pc-windows-msvc`, freestanding, no CRT; import
libraries for `kernel32` and `msvcr90` made by `llvm-dlltool` from generated `.def` files). Its
export table is generated from the user's own DLL at build time and not kept here: each of the
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
  the same way; its RB and LB cases were not aligned exactly). Not run yet. **Unconfirmed:** the
  mods agent's scripted left stick (four pushes right, two builds) did not change the 360's
  filter either; whether its scripting makes the `VK_PAD_LTHUMB_*` keystrokes is not known.
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
  360's English screenshots (`~/torchlight-gog/screens/side-en-*.png`): "town portal scroll",
  "mana potion", "health potion" take the 360's width and "Miscellaneous" fits its box. **[the run]**
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
  rare blue (`FFC0C0FF`), the unique sword in gold, "prefixed ring" and "prefixed ring" in green.
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
captures in play (`~/torchlight-gog/screens/ref/x360-ref-en-hud-still.png`, `-pet.png`; the mines floor 3): the orbs top left and right with a potion and its count over LB and RB, the
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
  `ITEM <a unique sword's name>,1` (unique, level 15), `ITEM <the identify scroll's name>,3`, `ITEM <the healing spell's name>,1`,
  `MONEY 500`), written by "Exit to Title". Its bag: two "dagger" unidentified, the unique sword unidentified, a common blade and the second blade equipped, a rifle, boots, unidentified
  shoulder armour and belt, the healing spell spell, identify scrolls, 600 gold.
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
- **identify scroll's card text** (right click, left click) is the item's own text in PC's
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
  rule and A `act.insert`; the tabs and the filter's name hidden (the list has no filter; what the
  360 shows there is not read: the mods agent is capturing it). A: the gem taken out of its bag
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
  kept and the markup taken out (`dialog_markup=1` keeps it). **Open, high priority:** the names'
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
- The pet or an NPC (03, "the pet"): PC's mouse-over does not give the pet, so the bar does not
  show it. Not known what makes the 360 show it.
- A unit killed stays in the bar, empty, for the rest of the 3 s the attacked one is shown (run
  41: "monster" empty). The 360's rule for a dead target is not read (`sub_821A83C8` is long).

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
3. **The object label of the candidate.** It is matched by text and, of several of one name, by
   the screen's centre (not the 360's rule). Next: project the candidate's position.
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
   - The Fast Scroll factor: `dialog_fast_scroll`, 4, chosen here and not measured. The 360's
     factor is `[r29+268]`, a value of the running game.
   - The gold hidden at the gem pages is not seen in a run.
   - The identify scroll's card text is PC's mouse text (the item's data, unchanged).
   - The card's price at the merchant is 3 pixels off (town/07).

**Untested for want of the means in the save:**
- An enchantment: done in run B.
- Removing gems (no socketed item; socketing with the pad is written since, not run).
- The Gambler's purchase: done in run B. Selling from the pet's bag: not tried.

**Earlier lists** (sections 11-13) still open:
- The converter must decrypt the player's XEX itself.
- The Skills page's requirement line; its X (the mapping) is written, not run (section 14).
- The target bar's size, checked here only for the enemy.

## 16. The Steam test: plan (for the user's approval, 2026-10-21)

**On hold (the user's decision, 2026-10-21):** adapting the port to Steam, and to the Epic
version, is thought about only once the interface port works on GOG. Until then this plan is a
draft, and `tools/port_addresses.py` is not run against another build.

Nothing here has been done; each phase starts only with the user's OK. Section 4's precautions
hold throughout, and `tools/steam_guard.py snapshot` runs before each phase that reads Steam's
folders, `verify` after it (UNCHANGED, or stop and tell the user).

**Why a plan is needed:** the proxy calls about 150 addresses of GOG's `Torchlight.exe` (152
constants in `proxy/*.c`; the CGameUI menus, the player's hotbar, the shops' actions, the pad's
movement). Steam's executable is another build (17685 against GOG's 17870, section 5), so none of
those addresses holds there as it is. With GOG's addresses the proxy would call into the wrong
code. The layouts, imagesets and fonts do carry over (section 5).

**Phase A, offline (reads Steam's install once, writes nothing there):**
1. Copy Steam's `Torchlight.exe` and `CEGUIBase.dll` to a folder outside the repository
   (`~/torchlight-steam/bin/`), with steam_guard around the copy. After that, only the copies are
   read.
2. `tools/port_addresses.py GOG_EXE STEAM_EXE --source proxy`: for each GOG address, the one
   place in Steam's build with the same bytes, once call displacements and in-image values are
   masked. Checked on GOG against itself: 152 of 152 map to themselves, all code. Every
   address it does not find, or finds by a short window, is located by hand in Steam's build,
   from its callers.
3. The struct offsets (CGameUI `+0x2d8..+0x31c`, the player's `+0x860`, the menus' fields) are
   compared through the code that uses them: a site matched byte for byte keeps the offsets
   written in it. An offset used only by the proxy is checked against a Steam site reading it.
4. The proxy gets one address table per build, chosen at load from `Torchlight.exe`'s PE
   timestamp. On an unknown build it hooks nothing and only the converted layouts load, so a
   wrong table cannot run. The CEGUIBase proxy is built from Steam's DLL, exports by name; the
   one import that differs (`UVector2(UDim, UDim)`) is forwarded like the rest.
5. Tests for the table choice, on synthetic PE headers. Committed: the table of addresses (they
   are numbers, not game files); not the copies.

**Phase B, one run (the user's approval, and the agents told before and after):**
1. A prefix of its own (`~/torchlight-steam/compat`), never `compatdata/41500`. The game folder
   is copied into `~/torchlight-steam/game/`, so the proxy never lands in Steam's install. The
   test save comes from GOG's prefix (the mine save), not from the user's Steam saves.
2. Steam closed (checked with `pgrep`, not assumed), Steam Cloud off for the game, and a
   `steam_appid.txt` with 41500 next to the copied executable, so Steamworks does not restart the
   game through Steam (`SteamAPI_RestartApp`, which would open it in the real prefix). The run
   script refuses `compatdata/41500` as `run_gog.sh` does, and runs without `steam` on the PATH.
3. If the game exits at once, or Steam starts: stop, `verify`, tell the user. No second try
   without a new plan.
4. The checks, short: the HUD, the inventory and Skills pages, a shop, an NPC dialog, a label,
   pad movement. Then quit, `verify` (UNCHANGED), and say "done".

**What may go wrong, known now:** Steam's executable needs `steam_api.dll` and may refuse to
start without a running Steam. If `steam_appid.txt` does not make it start, the Steam test stops
there and the user decides. Starting Steam would mean the real client near the user's prefix, and
that is outside this plan.

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
