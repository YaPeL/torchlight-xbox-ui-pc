# Regressions

Every bug that reached a hand pass (a QA run of the owner's) or a release, and every one caught only
after the change was taken as done (by a pull request's review or a run before the merge, where
the tests had passed): what happened, which layer should have caught it, and what catches it now (a
test, a log line, a test option or an item of the QA checklist at the end of this file). A new entry is added when a
bug is found that late; the lesson goes into a test, `AGENTS.md`, `REVIEW.md` or a lint, not only
here. Games' texts are named by role, never quoted.

## PC's HUDs flashing at each load (fixed in 617beb7)

- **What happened:** PC's bottom and pet HUDs showed for a frame at every load before the 360's
  HUD covered them, and PC's modal's 3D board was seen falling and rising around the 360's modal.
- **Should have been caught by:** a check of the first frames after a load, not only the settled
  screen.
- **Caught by:** the owner's QA runs 46 to 48.
- **Covered now:** the HUDs made transparent as their layouts load and the board kept hidden
  always (doc section 23). QA checklist: a level change and a load watched frame by frame, PC's
  HUDs never seen.

## PC's tip window seen before the 360's (fixed on the branch tip-hidden)

- **What happened:** at a new game the tip of the welcome showed PC's window for a moment, then the
  360's over it. The 360's replaced PC's only once `tip_tick` had found PC's window (a search every
  250 ms) and loaded its own layout.
- **Should have been caught by:** the rule the HUDs' flash left (a window PC shows is made
  transparent as its layout loads, not once it is seen); it was applied to the HUDs only.
- **Caught by:** the owner's run on the GOG copy (2026-10-24), after the first Windows test.
- **Covered now:** PC's `tipmenu.layout` transparent as it loads (`x360_pc_layout_loaded`). QA
  checklist: a new game, the welcome's tip watched frame by frame.
- **Seen in the same run:** the inventory's tip shown only once the 360's inventory closed: PC
  attaches it to its Top UI Sheet, which the 360's screens hide whole. Now detected by its own
  window's visibility and closed with its sheet shown for the click only; the pad its own over the
  inventory. QA checklist: a new character's first inventory, the tip in front, B closing only it.

## The hero played during a cinematic (fixed on the branch cinematic-pad)

- **What happened:** the pad moved the hero during a cinematic, which the 360's frame update does
  not allow (it skips the game's input while `sub_821ED810` holds).
- **Should have been caught by:** reading the 360's frame update whole before writing the pad's
  (its early skips, not only the stick's and the buttons' handling).
- **Caught by:** the owner's run on the GOG copy (2026-10-24).
- **Covered now:** `proxy/pad.c` asks PC's same test (`0x541db0` on CGameUI) before the world's
  input. QA checklist: a cinematic with the stick held and the buttons pressed: nothing acts.

## The game saving on Alt+F4 after a stop without the save hook (fixed in a45a7c6)

- **What happened:** with the save hook off, a stop asked the player to close the game, and
  Torchlight saves on closing, Alt+F4 too: QA run g1-3's save was written four seconds before the
  game ended, with the items PC had dropped.
- **Should have been caught by:** reading what PC does on closing before choosing how a stop ends.
- **Caught by:** the owner's QA run g1-3, from the save's time against the log.
- **Covered now:** without the save hook the load hook runs in stop-only mode (`load_cut_only`): the
  item is kept and the process ends at once, before any save; the notice no longer advises Alt+F4.
  The test options `test_save_hook_fails=1` and `test_load_hook_fails=1` run each case on a copy of
  a save, which is compared byte for byte after.

## The held A going through the black screen to the title's Continue (fixed in 7d62104)

- **What happened:** the A that closed the proxy's black screen was still held when the title came
  back, and the title's Continue took it.
- **Should have been caught by:** a rule for every screen the proxy shows itself: it keeps the pad
  until its closing button is let go, as the 360's modals do.
- **Caught by:** the owner's QA run g1-3b.
- **Covered now:** the black screen keeps the pad until A is released (and gives it back after A is
  held 2 s, bab761b). No automated test: QA checklist, the black screen closed with A held.

## The black screen's text not wrapped (fixed in 64c566e, after two tries)

- **What happened:** the stop's text ran off the screen on one line. The first fix set
  `HorzFormatting`, the second this scheme's `HorzTextFormatting` (7d62104, QA run g1-3b's next
  run); neither wrapped. The text is now laid out by `rich_text`, as the dialog's paragraphs.
- **Should have been caught by:** reading which properties the converted scheme's StaticText has
  before setting any (the window took both without an error and ignored them).
- **Caught by:** the owner's QA runs g1-3b and g1-4.
- **Covered now:** `rich_text` (WordWrapLeftAligned, each height measured). QA checklist: a stop's
  text at 1280x720 and on the Steam Deck.

## The mod's files written beside the game (fixed in 237ac2c and 6db1a42)

- **What happened:** `x360_stop.txt`, the log and the settings were written in the game's folder.
  Under Program Files, Windows refuses the write or redirects it into VirtualStore, where the
  uninstaller did not look.
- **Should have been caught by:** the review's rule that the mod writes only in its data folder.
- **Caught by:** the owner, reading the stop's design.
- **Covered now:** the files the mod writes at run time in `%LOCALAPPDATA%\x360ui` (`x360_data_path`,
  `x360_data_open`; beside the game or the DLL only as the documented fallbacks, doc section 24);
  `test_uninstall_removes_the_mods_data_folder` and `test_uninstall_removes_steams_prefix_data_folder`
  (Steam's Proton prefix); the uninstallers remove an older version's VirtualStore copy.
  `REVIEW.md` lists such a file written beside the game as a failure.

## `notices` printing CR CR LF on Windows (fixed in c890a46, before the merge)

- **What happened:** the texts `notices` prints were embedded from a Windows checkout with CRLF
  line ends, and the text-mode console stream turned each LF into CRLF again.
  `test_notices_carried` failed on Windows CI from the pull request's first commit (#16), and the
  pull request went to its second review with that check red.
- **Should have been caught by:** reading the pull request's checks before asking for a review.
- **Caught by:** Windows CI; seen when the checks were read before the merge.
- **Covered now:** `embed_blob.cmake` drops CRs from every embedded text; `test_notices_carried`
  on Windows CI, now a required check of `develop` and `main` (`AGENTS.md`: CI green before a
  review is asked).

## The saves backed up from the wrong prefix (fixed on the branch save-guard)

- **What happened:** before two runs on the GOG copy (2026-10-23), the agent backed up the saves of
  `~/torchlight-gog/prefix`, the first prefix (Wine alone, now only the game folder), whose save
  folder was empty. The game runs in `~/torchlight-gog/compat` and saves in its `pfx`; one of those
  runs changed a test character's save. Its earlier copy was in the installer's own backup
  (`save-backup-20261023-162202`) and was restored from it on 2026-10-24, the hashes checked.
- **Should have been caught by:** a backup step that finds the folder the game will use (the
  prefix the run is given, its `AppData` as Wine records it) and stops on an empty folder while
  another has saves, not one typed by hand.
- **Caught by:** the agent, reading the save's time against the runs.
- **Covered now:** `tools/run_gog.sh` calls `tools/save_guard.py backup` before every run: the save
  folder found from the run's prefix, copied to `save-backups/run-<time>`, the copy and the folder
  hashed again, and the run stopped if a hash differs, if the game is running, or if the folder
  found is empty while another folder under `~/torchlight-gog` has saves. Tests on synthetic
  prefixes (`SaveGuardTest`).

## QA checklist

What a QA run checks by hand, from the entries above, besides the change it is for:

- a level change and a load watched frame by frame: PC's HUDs and PC's modal's board never seen;
- a new game's welcome tip watched frame by frame: PC's tip window never seen;
- a new character's first inventory: its tip in front of it, B closing the tip only;
- a cinematic with the stick held and the buttons pressed: the hero does nothing until it ends;
- with no 360 screen open and no tip, nothing of the 360's tip shown (PC's Top UI Sheet hidden by PC
  itself, if it ever is, must not show it);
- the stop's black screen closed with A held: the title does not take the A;
- a stop's text on the black screen at 1280x720 and on the Steam Deck: wrapped, all of it seen;
- with `test_save_hook_fails=1` and with `test_load_hook_fails=1`, on a copy of a save: the game
  ends before any save, the save byte for byte the same after.
