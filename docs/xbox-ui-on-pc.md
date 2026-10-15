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
