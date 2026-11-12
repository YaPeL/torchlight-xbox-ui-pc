# An interface that does not depend on the 360's art (a plan, 2026-11-11)

A plan only: no code yet. Today the installer converts the 360's interface from the player's own
Xbox LIVE Arcade package. This document plans two ways of having an interface without it, which
can coexist:

- **a.** an interface with original art made with generative AI;
- **b.** a "skin" format so that an artist or modder can make one comfortably.

They share most of the work: b's format is what a's art is delivered in. The order at the end
does b's groundwork first.

The decisions that belong to the owner are listed in the last section.

## 1. What the mod takes from the 360's package today

The installer writes the converted interface to `x360ui` (and `x360ui-STYLE` for the other pads'
prompts) beside the game. `x360_ui.ini`'s `out=` names that folder and `prompts=auto` picks the
style. The proxy loads everything from `media/ui/x360/` inside it. Measured on the owner's
converted copy (the counts are of names and sizes, none of the content is quoted):

| Kind | What there is | Notes |
| --- | --- | --- |
| Textures | 6 DDS: 5 atlases of 1024x1024, 1 of 512x512 | about 6.5 MB |
| Imagesets | 6, with 336 images: `X360_widgets` 21, `X360_widgets1` 105, `X360_widgets2` 146, `X360_ceguiWidget` 52, `X360_ratings` 10, `X360_Loading` 2 | native resolution 1024x768, auto-scaled |
| PC's imagesets the 360's layouts name | `widgets3`-`widgets6`, `characters`, `1024X768` | PC's own art (item and skill icons, portraits): stays PC's |
| Fonts | 7 definitions on 2 TTF files: a sans family at 8, 11, 14, 16 and 32 points and a display face at 11 and 15 | the TTFs come from the package |
| Looks | 1 looknfeel with 62 widget looks, 1 scheme | the frames, buttons, bars and text styles |
| Layouts | 49 (1,815 windows); the proxy loads about 25 of them | the screens' structure |
| Texts | `x360_strings.txt`, read from the 360's executable and both games' tips | page names, prompts, tips |
| Pad prompts | the 360's button art; for the other pads, Kenney's icons (CC0, `data/prompts/`) drawn into the atlases | already an example of art that is not the 360's |

What the proxy relies on, the **contract** a replacement must keep:

- **Window names.** The proxy finds windows by name: at least 127 literal names in
  `proxy/x360_menu.c` (`win_of(PREFIX, "name")`), plus names built at run time (`ItemBkg1`-`3`,
  slots, rows). Many are the 360's anonymous names (`i_416`). A layout that renames or drops one
  breaks that screen's logic, not just its look.
- **Image names.** About 40 `set:... image:...` names are set by the proxy itself (rollover
  states, the selected tab, the bars' fills).
- **Window types and properties** the proxy reads or writes (text, visibility, area, alpha).
- **Fonts' names** (`X360_Serif`, `X360_Serif14`, `X360_SerifBig`, ...) and the letter spacing
  the proxy adjusts (doc section 11).
- **Text keys** in `x360_strings.txt` (`reward.xp`, `tip.ID`, ...).

So there are two levels of independence:

1. **The art** (textures, imagesets, fonts) replaced, the 360's layouts and texts still converted
   from the package. The player still needs the package.
2. **Everything** (art, layouts, looks, texts) replaced. The mod works without the package.

Level 1 is much smaller and is what a skin needs first. Level 2 is what "not depending on the 360"
fully means.

## 2. b. The skin format (for artists and modders)

### 2.1 What a skin is

A folder with a manifest, laid out like the converted folder:

```
skins/NAME/
  skin.ini                 name, author, license, version, the contract version it targets,
                           what it replaces (art, looks, layouts, texts), the fallback (x360 or none)
  media/ui/x360/*.png      the atlases (PNG; DDS optional), any size, power of two recommended
  media/ui/x360/*.imageset the rectangles, by the contract's image names
  media/ui/x360/*.font     the fonts' definitions, by the contract's font names
  media/ui/x360/*.ttf|otf  the font files (open licences only)
  media/ui/x360/*.looknfeel, *.layout, *.scheme   optional (level 2)
  strings.txt              optional: texts by key
  LICENSE.txt, CREDITS.txt
```

What a skin does not provide falls back to the converted 360 interface (when installed) or to the
built-in default skin (level 2, when made).

### 2.2 How it coexists with the 360's converted art

- The installer keeps converting the 360's interface into `x360ui` when a package is given.
- A skin installs into `x360ui-skins/NAME/` beside it.
- `x360_ui.ini` gets `skin=NAME` (empty: the 360's). The proxy builds the folder it loads from: the
  skin's files over the converted ones, file by file, so a skin can change only the buttons.
- The installer's window gets one more step: "Interface: the 360's (from your package) / a skin
  (list)". Without a package, only skins are offered (level 2).
- Pad prompts: a skin either ships its own per style (`prompts/xbox/...`) or keeps the converter's
  step that draws Kenney's icons into its atlases, which works on any atlas with the contract's
  prompt image names.
- `prompts=auto` and Nintendo's swap keep working: they choose among folders, as today.

### 2.3 Templates with exact measures

`tools/skin_template.py` (to write) reads the player's converted folder and writes a template
**outside the repository**:

- one PNG per atlas, transparent, with each image's rectangle outlined and named, at the atlas's
  size;
- `measures.csv`: atlas, image name, x, y, width, height, the native resolution, which layouts and
  windows use it, and whether the proxy sets it;
- one PNG per screen: the layout drawn as labelled boxes (the window names the proxy needs in
  red), from the layouts' areas at 1920x1080 and 1280x720.

Because the measures come from the player's own copy, nothing of the game is committed. For level
2 the contract's own manifest (names, sizes and the roles of each piece, written by us) replaces
it and can be published.

### 2.4 Preview and reload without restarting

- **In the game:** an ini option `skin_reload=1` (off by default) and a key (or the pad's Back +
  Start held): the proxy destroys and re-creates the imagesets and fonts from the skin's folder
  and reloads the open screen's layout. CEGUI's ImagesetManager and FontManager can do it at run
  time; the risk is windows that keep a pointer to an image (the proxy re-applies its images
  after the reload).
- **Outside the game:** `tools/skin_preview.py` draws a layout with a skin's imagesets to a PNG
  (the areas and images only, no CEGUI): enough to check placement and slicing without starting
  Torchlight. A watch mode redraws on save.

### 2.5 Documentation for artists

`docs/skins.md` (to write): the folder, the manifest, the contract's pieces by screen with their
measures and nine-slice rules (corners, edges, centre), the states (normal, rollover, pushed,
disabled), the text colours and markup the proxy uses, the resolutions to check, how to test
with the reload key, how to share a skin, and the licences accepted.

### 2.6 Fonts with open licences

The two TTFs come from the package and cannot ship. Candidates under the SIL Open Font License
(to choose by look and by coverage: Latin, the accents of the translations, digits that line up):
a sturdy sans for the body (sizes 8 to 16) and a display face for titles (11 and 15, and the 32
of big headings). The proxy's letter-spacing adjustment must become a per-font setting in the
font's definition, not a constant measured on the 360's fonts.

## 3. a. Original art made with generative AI

### 3.1 The assets needed

By piece, not by atlas (an original skin need not pack them as the 360 did):

| Group | Pieces | States |
| --- | --- | --- |
| Frames and panels | the page frame, the parchment panels, the dialog box, the item card, tips and modals | nine-slice |
| Buttons | the round gem button (small and large), tabs, list rows, arrows | normal, rollover, pushed, disabled |
| Bars | health and mana orbs and their fills, XP and fame bars, the target bars, the pet's | empty, fill, glow |
| The HUD | the skill bar's slots and frames, the minimap frame, the potions' slots | |
| Slots | inventory and equipment slots, socket marks, highlights, the rarity backgrounds | |
| Icons of the UI | the pages' icons (inventory, character, skills, quests, pet), the stat icons, the hourglass | |
| Screens | the loading screen's picture, the death screen's, the waypoint list's frame | |
| Pad prompts | Kenney's (CC0) as today, or drawn in the skin's style | per pad style |
| Fonts | two families (section 2.6) | |

Item, skill and monster icons and portraits stay PC's (the 360's layouts already use PC's sets).
Count: about 150 distinct pieces once the 360's duplicates (one image per state and per size) are
merged; the 336 images of the 360's sets include many states and variants.

### 3.2 A consistent style

- **A style guide** first (one page, written by a person): the tone (a lit, warm fantasy of brass,
  dark wood and parchment, readable from a couch), what to avoid (the 360's and PC's art, their
  logos, their lettering, Runic's and Microsoft's marks), the light's direction, the line weight,
  the texture amount.
- **A palette**: about 8 colours with their roles (frame, panel, text, highlight, health, mana,
  rarity tiers), checked for contrast on a TV at 720p.
- **Templates**: the nine-slice frames and the button made first, by hand or generated and then
  cleaned; every other piece generated from them (image-to-image, the same seed family and prompt
  base), so that the set shares its edges and light.
- **A review pass**: every piece compared beside the 360's and PC's at the same size, and anything
  that reads as a copy or a close imitation redone.
- **Provenance**: a log per piece (the tool and model, the date, the prompt, the seed, the human
  edits), kept in the skin's folder.

### 3.3 Licence of the result

- **The repository's code** stays GPL-3.0. Art is better under a licence made for it: CC BY 4.0
  (credit) or CC0 (none), in the skin's `LICENSE.txt`.
- **Who owns AI output** depends on the country. In the United States, the Copyright Office and
  the courts (Thaler v. Perlmutter, 2025) hold that material made by an AI without human
  authorship is not protected; the human parts (the selection, arrangement, editing and drawing)
  can be. Other countries differ. In practice: the pieces may not be protectable, and the licence
  covers what is. CC0 avoids claiming what may not be ours.
- **The tool's terms** decide what we may do with its output (commercial use, attribution,
  ownership). Use a tool whose terms let us publish the output under the licence chosen, and keep
  a copy of the terms of the date used.

### 3.4 Legal risks

- **Similarity:** output that resembles the 360's or PC's art, or another artist's work, can
  infringe whatever the tool. Mitigation: no prompt names Torchlight, Runic, the Xbox or an
  artist; no image of the games as an input; the review pass of 3.2.
- **Marks:** no Torchlight logo, no Xbox button glyphs in Microsoft's colours (the pad prompts stay
  Kenney's, as today), no font imitating the games'.
- **The models' training data:** pending lawsuits against some image models; the risk falls mostly
  on the providers, but a takedown of a model or a change of its terms can come later. The
  provenance log lets a piece be redone.
- **Distribution:** an AI skin can ship with the release (it has no game file), unlike the
  converted art. The check `tools/check_no_game_files.py` stays as it is; skins add a check that
  every file is listed in the skin's manifest with its licence.
- This is not legal advice; for a published release the owner may want a lawyer's read of 3.3 and
  3.4.

## 4. Level 2: without the package at all

What it adds to a skin:

- **Layouts and looks of our own**, written for the contract (the window names and types the proxy
  needs, our own geometry). Either written by hand from the contract or generated from the
  player's converted layouts by a tool that keeps only the names and types; the first is cleaner
  for publishing.
- **Texts of our own**: the keys of `x360_strings.txt` with our own English wording (and the
  translations PC has), so the 360's executable is not needed. The tips can be PC's (`pctip.ID`),
  read from the player's PC game as today.
- **The installer** without a package: it installs the proxy and the default skin.

## 5. Order of work and estimate

In working sessions of a few hours (one agent, reviews included):

| Step | What | Size | Risk |
| --- | --- | --- | --- |
| 1 | The contract written down: the windows, images, fonts and keys the proxy uses, by screen; a test that fails when the proxy uses a name not in it | 1-2 sessions | low |
| 2 | `skin=` in the ini and the proxy's merged folder (art only, level 1); the installers (Python and C++) install a skin folder | 2 sessions | low-medium (paths, both installers) |
| 3 | `skin_template.py` and `measures.csv` | 1-2 sessions | low |
| 4 | The in-game reload (`skin_reload=1`) | 2-3 sessions | medium (CEGUI objects freed while in use) |
| 5 | `skin_preview.py` | 2 sessions | low |
| 6 | `docs/skins.md` and a sample skin made of flat shapes (CC0), to test the format end to end | 1-2 sessions | low |
| 7 | Open fonts chosen and the letter spacing made per font | 1 session | low |
| 8 | a: the style guide, palette and templates; then the ~150 pieces generated, cleaned and reviewed | weeks of a person's eye; the agent can prepare prompts, slicing and checks | medium (legal, consistency) |
| 9 | Level 2: own layouts and looks for the ~25 screens the proxy loads; own texts; the installer without a package | 6-10 sessions | medium (every screen re-checked against the 360's captures) |

Steps 1-7 make the format usable by an artist (about 10-14 sessions). Step 8 runs in parallel once
6 exists. Step 9 is what makes the mod usable without the 360's package.

## 6. For the owner to decide

1. **Level:** only level 1 (art replaced, the package still needed) or level 2 as well?
2. **The skin's selection:** in the ini only, or also a step in the installer's window?
3. **The art's licence:** CC0 or CC BY 4.0 for the AI skin and the sample skin.
4. **The AI tool:** which generator (its terms decide 3.3), or whether a person draws the
   templates first.
5. **Publishing the contract's manifest** (names and sizes, written by us) in the repository, or
   generating it from each player's copy only.
6. **Fonts:** pick from a short list of OFL fonts that I would prepare with sample renders.
7. **Whether the AI skin ships in the release** next to the converter, or as a separate download.
