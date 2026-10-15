# torchlight-xbox-ui-pc

The Xbox 360 interface of Torchlight in the PC game: a study and tools.

This project does not include any file of the game, from the Xbox 360 or from PC. To use its tools
you need your own copies of both versions: the 360 game's data and the PC game's install. The tools
read those copies and write what they produce into folders outside this repository.

- `docs/xbox-ui-on-pc.md`: start at its section 0 (state, how to run GOG, precautions, next steps).
- `tools/pe_info.py`: sections, imports and exports of an .exe or .dll of the PC install.
- `tools/compare_installs.py`: the executables and DLLs of two PC installs (GOG, Steam), compared.
- `tools/layout_windows.py`: the named windows of a CEGUI layout, and which of them another has.
- `tools/convert_x360_ui.py`: the 360 interface's files (scheme, looknfeel, imagesets, fonts,
  layouts) converted from the user's 360 `pak.zip` into a folder for the PC game.
- `tools/steam_guard.py`: hash snapshot and verify of the Steam folders that must not change.
- `tools/run_gog.sh`: runs the GOG copy through Proton's script in a prefix of its own.
- `tools/check_no_game_files.py`: run by CI; fails if a game file type is committed.

Tests use synthetic data only.
