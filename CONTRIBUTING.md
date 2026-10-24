# Contributing

Thank you for helping. A few things to know before opening a pull request:

- **Target `develop`.** Work is integrated on the `develop` branch; `main` only receives releases.
  Make your branch from `develop` and open the pull request against `develop`. Both branches are
  protected: changes go in by pull request with CI green.
- **No game content.** Never commit anything taken from the game, from the Xbox 360 or PC, or
  derived from its files: assets, executables, extracted data, captures, saves, logs of runs, or
  text copied from the game. The tools convert from the player's own copies into folders outside
  the repository, and tests use synthetic data only; CI checks it
  (`tools/check_no_game_files.py`). Pull requests that add game content are closed.
- **The 360 is the reference.** What the pad does in play and in the 360's screens follows what the
  360's code does, not PC's mouse; say which 360 function you read.
- **Both installers.** A change to `tools/install.py` is made in `installer/src/install.cpp` too,
  and the other way round; `tests/test_installer_cpp.py` checks that they agree.
- **English** for code, comments, documentation and commit messages.
- **Tests.** CI runs the tools' tests, the proxy's build and the installer on Linux and Windows for
  every pull request. Say how you checked your change (the pull request's template asks).
- **Licenses.** The project's code is GPL-3.0 ([LICENSE](LICENSE)). Code taken from elsewhere keeps
  its license and says where it came from (`installer/third_party/NOTICES.md`). By contributing you
  agree that your contribution is licensed under the same terms.

How a change is reviewed: [REVIEW.md](REVIEW.md). How the mod works: [docs/xbox-ui-on-pc.md](docs/xbox-ui-on-pc.md).
