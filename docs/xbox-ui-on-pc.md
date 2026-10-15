# The 360 interface in the PC game: feasibility (study, 2026-10-15)

This study was written in the recomp's repository (`torchlight-recomp`), where `docs/pc-ui.md`,
`docs/mods.md` and `game_menu/wide_layout.h` live; references to them below are to that repository.
No game file, from the 360 or from PC, goes into this one: only tools that read the user's own
copies (`tools/`). PC was not run for this study; everything is read from the user's files (PC's
`Pak.zip`, `Torchlight.exe` and its DLLs, the Xbox pak and the decrypted Xbox image). Marks:
**[read]** (read in the files), **[inferred]**, **[to verify]**.

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

## 4. Running the PC game: precautions (not done; needs the user's OK)

The work is done on the GOG version, in a prefix of its own (section 5); the Steam version is
tried last, with the precautions below.

Nothing in this study needed PC to run. Stage 1 onwards does. Before any run:

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
- Two hypotheses failed, so this stopped here. Next candidates, for the user to choose:
  - run the copy through Proton's own launcher script with the separate prefix
    (`STEAM_COMPAT_DATA_PATH` set to a folder of ours), which sets the runtime and the DLLs as for
    Steam games, still without Steam;
  - Wayland/XWayland's window size: try a virtual desktop
    (`explorer /desktop=TL,1280x720`);
  - `FULLSCREEN :1` in `local_settings.txt`.
