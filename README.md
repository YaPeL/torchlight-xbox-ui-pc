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
3. **Modal windows:** PC's tips as the 360's `tipmenu` (B closes, Y turns them off) and PC's
   modal (the fishing's result, the hardcore death) as the 360's `modalmenu` (A accepts or
   oks, B declines): written, not run yet (section 14).
4. **Whirlpool ("remolino"):** the user's description: on the 360 a fast spin of the stick turns
   the hero in place, on PC it walks a spiral. The 360's stick path matches the port's (section
   9e); set aside unless the mods agent knows where the 360's engine decides to turn in place.
5. **Skills on B/Y/LT/RT:** the HUD shows them; the 360's mapping on the Skills page (X, then the
   button, assigned on its release) is written and not run yet (section 14).
6. **Steam and Epic:** only once the port works on GOG (the user's decision, 2026-10-21). A
   draft plan for Steam is in section 16; its first offline tool, `tools/port_addresses.py`, is
   written and tested on synthetic data, not run against another build.
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
- `tools/port_addresses.py`: GOG's addresses found in another build of `Torchlight.exe`, offline.
- `tools/steam_guard.py`: hash snapshot and verify of the Steam folders that must not change.
- `tools/run_gog.sh`: runs the GOG copy through Proton's script in a prefix of its own.
- `tools/check_no_game_files.py`: run by CI; fails if a game file type is committed.

Tests use synthetic data only.

## License

GPL-3.0 (LICENSE). The code only: no file of either game is in this repository, and the converted
interface is made from your own copies on your machine. The prompt icons in data/prompts are
Kenney's "Input Prompts" (CC0; data/prompts/LICENSE.txt).

## Installing

`python3 tools/install.py` finds the PC game (GOG, Steam, Epic) and your extracted 360 copy, converts
the 360's interface into the game's folder (one copy per pad style: Xbox, PlayStation 3/4/5,
Nintendo) and installs the proxy. Only GOG's build 17870 is supported for now; `--list` shows what
it finds, `--remove` puts the real CEGUIBase.dll back. It needs the 360's decrypted image
(`default.bin`); without one it uses 360tools' `extract_pe.py` if installed.
