# The plan to 1.0 (2026-11-12)

What is left between v0.8.2 and 1.0, in the order proposed, with each item's size, risk and how it
is checked against the 360. Sizes are in working sessions of a few hours of one agent, reviews and
runs included.

The version rule (the owner's): 1.0 minus 0.1 for each 1.0 item still missing, plus 0.0.1 for each
bug fixed. v0.8.2 has two left: **the main and pause menus** and **the loading screens**. The
other items below are known limitations and bugs; they do not move the first digit but 1.0 should
not ship with them open.

## How an item is checked against the 360

Every item follows the path the death and waypoint menus took (doc section 56):

1. **[read]** The 360's code, read by the recomp agent in the recompiled build (read only): the
   class, the functions and the rule, with their addresses, written in the doc before the code.
2. **[measured]** Captures of the 360 running (the recomp, at 1280x720 or 1920x1080) for the
   look: positions, sizes, colours, fonts, and the sequence of screens.
3. **[port]** The port reproduces the rule with PC's functions as the means, nothing added.
   Every address used is verified on GOG's executable and on Steam's (`addresses_steam.txt`); both
   builds compile and `tests/test_tools.py` passes.
4. **[run]** A scripted GOG run under Proton on an isolated save (Steam UNCHANGED, everything put
   back), its captures beside the 360's (`tools/compare_ui.py` for the layout dumps).
5. **[played]** The owner plays it with a pad (GOG; Steam before the release). An item the owner
   reports working is closed.

## The items, in the proposed order

| # | Item | Size | Risk | 1.0 item |
| --- | --- | --- | --- | --- |
| 1 | Enchanting with a full bag | 1 | low | bug (PR #134) |
| 2 | The quest reward's card width | 2-3 | low | limitation (stopped: cause not found) |
| 3 | The loading screens | 3-5 | medium | **yes** |
| 4 | The pause menu | 4-6 | medium | **yes** (with 5) |
| 5 | The main menu (the front end) | 8-12 | high | **yes** (with 4) |
| 6 | The installer's check of the saves before changing the bag | 3-4 | medium | limitation |
| 7 | The waygate's step | - | - | waits for the owner's 360 run |
| 8 | Steam's older build | - | - | a check per release, not work |
| 9 | Polish left from the catalog's lists | 2-4 | low | triage |

Total: about 22-34 sessions before the owner's played runs.

### 1. Enchanting with a full bag (size 1, risk low)

- **[read, 360]** (the recomp agent, 2026-11-11) The enchant button (`sub_8236B348`) takes the
  selected item (`sub_82369F60`; the list holds equipped and bag items), checks the gold and calls
  `sub_82361C78`, which changes the item **where it is**: one path pays (`sub_82361E80`, with
  `sub_8229FB58`), the other does not (`sub_823629D0`, the shrine's, inferred). The affixes are
  removed and added (`sub_822B5E88`, `sub_822B5D10`), the enchant count `+792` goes up, then the
  player's stats are refreshed. It never takes the item out of its slot, never puts it in the bag
  and never drops it. A full bag cannot matter on the 360.
- **[read, PC]** PC's act (CEnchantMenu's `0x57a800`) works on the item in the hero's slot 14,
  then takes it out (`0x4e9ca0`) and picks it up into the bag (`0x4e9a30`, at `0x57b619`),
  dropping it at the hero (`0x4f5960`) when it fits nowhere. The proxy puts the selected item in
  slot 14 for the act and back in its place after it (the doc's enchant shrine notes, "The
  item's place").
- **[run, 2026-11-12]** GOG, the synthetic shrine save, PC's bag filled to 21/21 with the
  console (`SPAWNCLASS`, `X360PICKUP`), the hero's equipped sword enchanted at the shrine: the
  sword was enchanted and stayed equipped, the bag still 21/21, nothing on the floor. PC's pickup
  puts an item back in its empty equipment slot first, so an equipped item comes back. A bag item
  frees its own slot when it goes to slot 14, so it comes back too.
- **Open:** an item from the **pet's bag** with the hero's bag full would fit nowhere and be
  dropped at the hero. Not run: two tries to set it up failed (the item moved to the pet was
  unidentified, so the page refused it; the bag was one short). Stopped there by the
  two-tries rule.
- **[port]** Pull request #134: PC's act has one pickup per mode (the enchanter's, the shrine's and
  the gem goblin's that keeps the item); the three calls are hooked so that, during the proxy's
  act, an item that fits nowhere is kept (saves held) and given back to its place. Built for GOG
  and Steam; the hooks install and let the act through in a run; the full-bag case itself was not
  reached in a run (three ways to set it up failed), so it has steps for a hand test.
- **What PC does without the mod:** the item is dragged into the enchant slot from the bag, which
  frees its bag slot, so the vanilla game does not drop it in practice either.

### 2. The quest reward's card width (size 1-2, risk low)

- **[read, 360]** (the recomp agent) The quest dialog (`sub_82358058`), the Quest page
  (`sub_8237E4D8` -> `sub_82344B90`) and the inventory (`sub_82367D38` -> `sub_82344B90`) use
  one card (the UI manager `0x835594F8`, its `+884` loaded from `tooltip.layout`); none passes a
  width. The card is sized each frame (`sub_8233F318` -> `sub_8233FE48`): each text block is
  measured word-wrapped centred in a rect of 1000 (`0x820AA2C0`'s `+172`; the rect's order is
  inferred), each line's window is its extent + 4, and the card is the widest line plus padding.
  So the 360's card grows with its text up to about 1000 units before it wraps.
- **[measured]** The port's reward card was 446 px wide at 1920x1080 against the 360's 786 (doc
  section 37): PC's tooltip wraps its text narrower, so the card is narrower and taller.
- **[read, PC, 2026-11-12]** PC's equipment tooltip (GOG `0x541e80`) measures the same way: the
  non-description blocks Centred in a rect 1000 wide, each extent + 4, the widest kept; the
  description WordWrapLeftAligned in a rect 1.5 times the widest line (`0xaa4d04` = 1.5; the
  calls at `0x54656c`, `0x546929`; Steam `0x541f96`, `0x542341`).
- **[read, 360]** (the recomp agent's follow-up) The 360's description block is the same: 1.5
  times the running widest line (`sub_8233FE48`, 0x82342F50); only its name block differs, word
  wrapped centred in 1000.
- **Stopped (two hypotheses failed):** the wrap width (1000) and the description's factor (1.5)
  are the same in both games, so neither explains 446 px against 786. What is left to compare:
  the content measured before the description (which blocks the 360 builds for a reward item, and
  their texts), the fonts' sizes at the measure, and the layout's padding. Next step: a dump of
  both cards' windows for the same item (the port's `x360_dump`, a recomp capture with its
  window rects) before any code.

### 3. The loading screens (size 3-5, risk medium) — 1.0 item

- **[read, 360]** (the recomp agent, 2026-11-11)
  - `loading.uilayout`, made by CGameUI's create-menus (`sub_823392E8`): the root at CGameUI
    `+0x344`, `TipText` `+0x348`, `HOURGLASS` `+0x364`.
  - Shown and hidden by `sub_82338218(CGameUI*, bool)` (11 callers: the state change
    `sub_82212950`, the main loop, the main menu's commands). Shown, it picks a tip.
  - Pumped by `sub_82337D58` from 46 places in the level load's steps: the tip changes when more
    than 6000 ms passed (it re-rolls until another tip comes), and the hourglass pulses: a phase
    `+880` grows by 1.0 per call and the image's alpha is `sin(phase) * 0.5 + 0.5` (sine
    inferred), written to `HOURGLASS`'s `ImageColours`. So it pulses once per load step, not on a
    timer.
  - The tips: `media/globals.dat`'s `LOADINGTIPS` (one `TIP` each), drawn at random with weights,
    a shown tip's weight lowered (`sub_82391370`, `sub_82391248`).
  - The picture: one image (`set:Loading image:loading`), the same for every area.
  - The ratings and Xbox LIVE Arcade images show 3 s each at the first boot only: not ported (the
    console's).
- **[port]**
  - The converters (Python and C++, with a parity fixture) write the 360's loading tips as
    `loadtip.N` keys in `x360_strings.txt`, as the context tips are now.
  - The proxy shows the 360's `loading.layout` over PC's loading screen while PC loads (a level
    change, a portal, the first load) and hides it when play starts; PC's own loading picture and
    tip are hidden under it.
  - The pulse and the tip's change follow PC's load steps: the place to hook is PC's own call that
    redraws during a load (to find in GOG's executable: the counterpart of the 360's
    `sub_82337D58`), with Steam's address.
- **Risk:** PC draws during a load from its own loop; a window shown at the wrong moment can stay
  over play, and a hook in the load path can stall it. The first step is a diagnosis run that
  only logs the calls.
- **[measured]** A recomp capture of a load (the picture, the tip's place, the hourglass) and one
  at a tip change.

### 4. The pause menu (size 4-6, risk medium) — 1.0 item, with 5

- **Where it stands:** the 360's pause menu is written (`proxy/x360_menu.c`, "the pause menu":
  `optionsmenu.layout`, Help with the how-to screen) but Start opens PC's pause menu since
  2026-10-23 (the owner's choice for the first beta: the 360's menu over PC's screens left a
  window with no way out).
- **[read, 360]** (the recomp agent) COptionsMenu (`sub_82353870`, at CGameUI `+932`; OnCommand
  `sub_82353BE8`): Options (94) toggles the settings menu (`sub_8233D620`) and closes; Help (100)
  the how-to menu; Achievements (97) and Leaderboards (96) are Xbox LIVE's; Exit to Title (0)
  clears CGameUI `+6024`/`+6028`, calls `sub_82372888(3, 0)` (a stats flush, likely) and closes;
  Unlock Full Game (99) and Gift a Friend (14) are the trial's and LIVE's; Close (6).
- **[port]**
  - Start opens the 360's menu again, with Options, Help and Exit to Title.
  - **Options:** the 360's settings screen (`settingsmenu_xenon.layout`) with what PC can do
    (the volumes, the brightness and contrast already ported in doc section 45, the vibration),
    or PC's settings walked with the pad as now. To decide (below).
  - **Exit to Title:** PC's own exit to the title (the save PC makes on it kept as it is).
  - Achievements and Leaderboards hidden (no Xbox LIVE on PC), as Unlock Full Game and Gift a
    Friend are now.
- **Risk:** the exit path saves the game; a menu left open over PC's screens (the 2026-10-23
  problem) is what to test first.

### 5. The main menu, the front end (size 8-12, risk high) — 1.0 item, with 4

- **The owner's earlier decision** (2026-10-17) left the front end PC's; the 1.0 list counts it.
  To confirm before starting.
- **[read, 360]** (the recomp agent) CMainMenu (`mainmenu.uilayout`; vtable `0x820D2444`; Init
  `sub_8238BEE8`, OnCommand `sub_8238CB98`), inside CMenuManager (CGameUI `+1004`) with
  CNewGameMenu (`+3460`), CDifficultyMenu (`+3464`) and CContinueGameMenu (`+3468`);
  SetActiveMenu `sub_82345FB0`. Its entries: Continue (the last character), New Character, Load
  Character (only with characters), Achievements, Leaderboards, Help, Options, Unlock Full Game
  and Exit. Not decoded yet: the "press A" screen and the credits inside Update/SetOpen.
  `mastermenu_xenon.uilayout` is never loaded by the 360 (a leftover).
- **[port]** By screen, each its own pull request: the main menu over PC's title (its commands
  calling PC's own: continue, new, load, settings, exit); then load character (the 360's list of
  characters over PC's); then new character and difficulty (the class, the pet, the name typed
  with PC's text box, as the 360's keyboard is the console's).
- **Risk:** the front end creates, loads and deletes characters: a wrong command can delete a
  save. Every step runs on copies with the save hashes checked.

### 6. The installer's check of the saves before changing the bag (size 3-4, risk medium)

- README: "A check of the saves in the installer, before changing the bag, is still to come."
- **[port]** Before installing PC's bag over the 360's (or uninstalling with the 360's bag used),
  the installers read each save (`tools/save_items.py`'s partial reading, ported to C++) and name
  the characters whose bag holds more than PC's bag takes, so the player knows which to load
  first. Read only.
- **Risk:** the save format is read partially; a wrong count could reassure. The check only warns
  and never decides; the backups stay as they are.

### 7. The waygate's step (no work until the 360 is measured)

The owner measures in the recomp whether the 360's hero climbs the dungeon gate's step with the
stick. If it does and A takes the gate there, a fix is planned then. Until then the README line
stays.

### 8. Steam's older build

Steam's build (17685) is not something the mod can change. Each release: the Steam proxy built
and its addresses checked (CI), and a run on Steam before the release by the owner.

### 9. Polish from the earlier lists (size 2-4, triage first)

Items noted in the doc's earlier lists that are not in the README's limitations, to re-check in one
owner-played run and close or plan: the Quest page's stack count and rarity background (section
48), the reward icon drawn larger than the 360's, the object label's background, the merchant's
side panels, PC's first-time tips after a shop, the fishing result window, the minimap's corner
overlay seen by one user (#88).

## Open issues on GitHub

- #89 (aiming) and #112-#114: fixes merged; they wait for their reporters or the
  owner to close them.
- #105 (a second weapon set): not on the 360; after 1.0.

## For the owner to decide

1. The main menu and the front end (item 5): port them for 1.0, as the 1.0 list says, or keep
   PC's (the 2026-10-17 decision)?
2. The pause menu's Options (item 4): the 360's settings screen, or PC's walked with the pad?
3. Achievements and Leaderboards in the 360's menus: hidden (proposed), or shown and refused?
4. The order: smallest first (as above), or the 1.0 items first?
