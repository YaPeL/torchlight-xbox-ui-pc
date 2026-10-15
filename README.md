# torchlight-xbox-ui-pc

The Xbox 360 interface of Torchlight in the PC game: a study and tools.

This project does not include any file of the game, from the Xbox 360 or from PC. To use its tools
you need your own copies of both versions: the 360 game's data and the PC game's install. The tools
read those copies and write what they produce into folders outside this repository.

- `docs/xbox-ui-on-pc.md`: feasibility study, plan by stages and cost.
- `tools/`: scripts that read the user's copies (no game data inside).
- `tools/check_no_game_files.py`: run by CI; fails if a game file type is committed.

Tests use synthetic data only.
