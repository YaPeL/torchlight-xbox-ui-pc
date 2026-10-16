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
at that death.

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
- **Scale:** the 360 draws it for 720 rows in absolute pixels, and CEGUI 0.6 does not scale
  them (the fonts do). On load every window's `UnifiedAreaRect` offsets are multiplied by the
  game's height / 720 (1.5 at 1080). The 360's tabbed frame (`mastermenu_xenon`) places the panel;
  here it is moved to where the 360 shows the bag, the frame's middle column ((752, 128) at
  1080 in the 360's screenshots, `~/torchlight-gog/screens/ref/`). **[the runs; the screenshots]**

**What the 360 does (read in the image):**
- **Buttons** (`sub_8236FEF0`, `sub_8236A810`): d-pad left/right toggle the Info view; A acts
  (`sub_8236AA48`); X sells (`sub_8236AD18`, in a shop); Y drops (`sub_82369E80`) or transmutes;
  RB/LB the filter (`sub_8236A488`: 0 to 5, past the ends the housing's next screen); B closes.
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

**Not done yet** (noted for later): the rows' damage and armour, requirements, enchantments,
quality colour, price, sockets and comparison arrow; the Info view; X (sell) and Y (drop); the
transmuter; the housing's tabs (LT/RT) and the frame; the identify scrolls' window; the pet's
inventory; the 3D doll and the comparison panel are not part of this screen on the 360.
