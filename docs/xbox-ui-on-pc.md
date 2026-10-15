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
| GOG saves, settings, mods, `Ogre.log` | `~/torchlight-gog/compat/pfx/drive_c/users/steamuser/AppData/Roaming/Runic Games/Torchlight/` (`save/`, `mods/`, `settings.txt`, `local_settings.txt`, `resourceconfig.dat`) |
| Proton's logs of our runs | `~/torchlight-gog/steam-gogtl.log` |
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
3. The game opens at 1920x1080 (the desktop's size; `local_settings.txt` has `WINDOW_WIDTH`,
   `RES_WIDTH`, `FULLSCREEN`) with DXVK, and reaches the main menu. Close it when done: Alt+F4 or
   the game's own Quit.
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
  paks and run on the user's 360 pak. Nothing of it has been loaded by the PC game yet; that is
  stage 1.

**Next, in order (section 3's stages):**
1. **Stage 1, the proxy `CEGUIBase.dll`** (x86, exports forwarded by name to the real DLL, renamed):
   - first only log the layouts and schemes PC asks for;
   - then load the 360 scheme (`media/ui/x360/ceguiWidgets.scheme`, from stage 0) after PC's.
   - The toolchain is not on this machine: `clang-cl` and `llvm-dlltool` are, `lld-link` and
     mingw-w64 are not. A proxy that forwards by name in a `.def` file and intercepts a few
     members with `__thiscall` thunks needs no MSVC C++ ABI and no CRT, only a linker.
   - The converted files must reach PC's resource groups: the proxy adding `OUT` as a resource
     location, or a folder under the GOG prefix's `mods/` **[to verify: whether PC's mod loading
     takes new paths, not only replacements]**.
   - The proxy goes into the copy's game folder only.
2. Stages 2 to 5 as in section 3.

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
