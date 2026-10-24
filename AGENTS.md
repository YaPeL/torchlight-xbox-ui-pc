# Working on torchlight-xbox-ui-pc

The Xbox 360 interface of Torchlight in the PC game, from the user's own copies. Start with
`docs/xbox-ui-on-pc.md`, section 0. "The owner" is the repository's owner; an OK this file asks for
comes from the owner directly, not relayed by another agent.

## The reference for movement, combat and input is the 360's code

For anything the pad does in play (moving, stopping, turning, choosing a target, attacking,
picking up, using skills and potions) and in the 360's screens, read what the 360 build does
first (the recompiled code and the image in `~/torchlight-recomp`, read only) and reproduce
that, using PC's functions only as the means. Do not copy PC's mouse behaviour where the 360
does something else, and do not add rules the 360 does not have. State in the doc the 360
function and the rule read.

## Running the PC game

- Work on the GOG copy only, in its own prefix (`~/torchlight-gog/compat`, through
  `tools/run_gog.sh`). Never use Steam's install, `compatdata/41500` or `userdata/<id>/41500`.
- Before every run, snapshot Steam's folders with `tools/steam_guard.py snapshot`; after it,
  `verify` must print UNCHANGED. If anything changed, stop and tell the user.
- The Steam version is tried last, with a plan the user approves (section 4 of the doc).
- Ask before launching the game. When the user plays a run, do not touch the game (no input,
  screenshots or other runs) until `Torchlight.exe` ends.
- The owner's saves are never touched; a run that could change one uses a backup first.
- Agents that share the machine coordinate before using it: ask before a build or a run while
  another agent may be measuring or running.

## The repository

- Work in `~/torchlight-xbox-ui-pc-public`, this public repository's clone. `~/torchlight-xbox-ui-pc` is
  only a copy of the archived private repository: never add this repository's remote to it.
- No file of the game, from the 360 or PC, is ever committed: assets, executables, extracted
  data, captures, saves, run logs, or text copied from the game (CI checks it with
  `tools/check_no_game_files.py`; do not weaken it). Tools convert from the user's copies into
  folders outside the repository; tests use synthetic data only. In code and docs, a game text
  is named by its key and address, never quoted.
- Small commits on branches; `main` stays green. One commit per logical unit.
- Before integrating a branch into `main`, have it reviewed by a subagent with a clean context that
  gets only the diff and the task's description; an important finding is fixed or justified first.
- Never claim support that is not verified (README, docs, release notes): a store's build is
  "supported" only once it was played with the mod.
- Code taken from elsewhere keeps its license and says where it came from (`tools/xex/README.md`).
- Docs, commits and code in English; replies to the user in Spanish (voseo).

## OpenTorchlight

`github.com/Qweasd123tg/OpenTorchlight` (a decompilation of the Linux build, no license) is read
only, cloned outside this repository (`~/ext/OpenTorchlight`): never run its scripts, never copy
its code or text. A name taken from it is marked *(OT)* in the doc; every function is located and
verified in our own binaries (its addresses are of another build).

## Way of working

- **Two hypotheses:** if two hypotheses in a row fail on one point, stop, write down what was
  found, and tell the user.
- When an item is reported working by the user, it is closed: it is not asked again unless the
  user reports a regression.
- When closing a task, report in a few lines what changed, how it was verified and what is still
  open.
- **Turns:** an agent only works during a turn. Never end one promising to go on ("sigo con X"):
  do it in the same turn, or say plainly that you stopped and why. When waiting for the owner,
  say so in the last line.
