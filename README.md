# torchlight-xbox-ui-pc

The Xbox 360 interface of Torchlight in the PC game: a study and tools.

This project does not include any file of the game, from the Xbox 360 or from PC. To use its tools
you need your own copies of both versions: the 360 game's data and the PC game's install. The tools
read those copies and write what they produce into folders outside this repository.

## Paused (2026-10-17)

The project is paused until the recomp is finished. Where it stopped: branch `inventory-x360`, all
pushed. Every screen of the 360 catalog in scope is ported to the GOG copy and compared against
the 360's (`docs/xbox-ui-on-pc.md`, sections 14 and 15). What comes next, in the user's list:

1. **Transmuter:** done and tested with one recipe; what remains is in section 15.
2. **Shop (merchant):** PC's panels at the screen's sides, and PC's first-time Tips after
   closing.
3. **Modal windows:** PC's modal windows (Tips, the fishing result "You caught …", confirmations)
   are still PC's; the 360 has `modalmenu.layout` and `tipmenu.layout`.
4. **Whirlpool ("remolino"):** what this item refers to was not written down when the project
   paused. The near candidates are PC's fishing ripples under the 360's ring (section 15, item 7)
   and the town portal's swirl. Confirm with the user before working on it.
5. **Skills on B/Y/LT/RT:** the HUD shows them, but they were not tried with skills assigned
   (section 9).
6. **The Steam test**, last, with a plan the user approves (section 4).
7. **QA with the 8BitDo pad** (the user's controller), the whole flow played.

Section 15 of the doc has the full list of what is left (first: the gold of names in the
dialogs, stopped after three tries, its next steps written there).

- `docs/xbox-ui-on-pc.md`: start at its section 0 (state, how to run GOG, precautions, next steps).
- `tools/pe_info.py`: sections, imports and exports of an .exe or .dll of the PC install.
- `tools/compare_installs.py`: the executables and DLLs of two PC installs (GOG, Steam), compared.
- `tools/layout_windows.py`: the named windows of a CEGUI layout, and which of them another has.
- `tools/convert_x360_ui.py`: the 360 interface's files (scheme, looknfeel, imagesets, fonts,
  layouts) converted from the user's 360 `pak.zip` into a folder for the PC game.
- `tools/build_proxy.py`: builds the proxy `CEGUIBase.dll` (`proxy/cegui_proxy.c`) from the user's DLL.
- `tools/grab_window.py`: a screenshot of the game's window.
- `tools/send_input.py`: mouse and keyboard input for the game, from inside its Wine prefix.
- `tools/steam_guard.py`: hash snapshot and verify of the Steam folders that must not change.
- `tools/run_gog.sh`: runs the GOG copy through Proton's script in a prefix of its own.
- `tools/check_no_game_files.py`: run by CI; fails if a game file type is committed.

Tests use synthetic data only.
