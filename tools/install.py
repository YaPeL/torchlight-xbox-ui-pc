#!/usr/bin/env python3
"""Installs the Xbox 360 interface into the user's PC Torchlight, from the user's own 360 package.

    install.py [install] [--package FILE | --x360-dir DIR] [--game DIR] [--bag pc|360] [--yes] [--console]
               [--experimental]
    install.py uninstall [--game DIR] [--yes] [--console] [--anyway]
    install.py list

Install, as the recomp's first start does it (its src/game_setup):
1. The PC game: --game, or the one found (GOG: Heroic's, Lutris's or any Wine prefix's
   "drive_c/GOG Games/Torchlight", GOG Galaxy's registry entry, the usual folders; Steam: its library
   folders; Epic: Legendary's and Heroic's lists, the launcher's manifests). Its build is told by
   Torchlight.exe's timestamp; only a build the proxy was made for is installed into.
2. The 360 copy: the Torchlight Xbox LIVE Arcade package (an STFS LIVE/CON/PIRS file of title
   58410A7E, read with tools/stfs.py), or a folder extracted from it; asked for with a file dialog
   when not given. Its default.xex is decrypted and decompressed (tools/xex) for the 360's texts and
   its pak.zip read for the interface. Nothing is copied into this repository or sent anywhere.
3. The bag: PC's (three sections of 21 slots) or the 360's (one of 50 for every kind of item). The
   360's changes where a save's items go, so it is asked with a warning, and the saves found are
   backed up first.
4. The 360's interface converted into the game's folder: x360ui (Xbox prompts) and x360ui-STYLE for
   playstation3, playstation4, playstation5, nintendo and nintendo-swapped (A and B swapped: the
   proxy's Nintendo layout, set in PC's settings screen); x360_ui.ini written (its other lines
   kept); the proxy CEGUIBase.dll installed, the real one kept as CEGUIBase_orig.dll.

Uninstall puts the real CEGUIBase.dll back and removes what the install wrote (the converted
folders, x360_ui.ini, the mod's logs). Saves are not touched. With the 360's bag installed it warns
first (the original game deletes the items that do not fit PC's bag) and offers PC's bag instead
(the default; then load and save each character, and uninstall); uninstalling anyway (a yes, or
--anyway) backs the saves up first.
Once the 360's bag was used, uninstall with PC's bag back asks whether each character was loaded
and saved with it (no by default, --yes included; --anyway answers yes) and backs the saves up.

The questions are asked in windows (tkinter) when there is a display, else on the console
(--console forces it); --yes takes the defaults (PC's bag; at uninstall with the 360's bag, PC's
bag installed instead) without asking.
"""
import datetime
import glob
import json
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(HERE)
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "xex"))
import build_proxy  # noqa: E402
import cegui_exports  # noqa: E402
import convert_x360_ui  # noqa: E402
import stfs  # noqa: E402

GOG_TIMESTAMP = 0x4FFC6815        # Torchlight.exe of GOG's build 17870, the one the proxy knows
STEAM_APP = "41500"
STYLES = ("playstation3", "playstation4", "playstation5", "nintendo", "nintendo-swapped")   # the last: A and B swapped
X360_LAYOUT = "media/ui/mastermenu_xenon.layout"     # a file only the 360's pak has
X360_TITLE = 0x58410A7E                               # Torchlight's Xbox LIVE Arcade title ID
X360_CONTENT_TYPE = 0x000D0000                        # an Arcade title's package
PROXY_FILES = {"gog": "CEGUIBase.dll", "steam": "CEGUIBase-steam.dll"}   # a release's proxy per build, next to this script
EXPERIMENTAL = {"steam"}   # ported offline, not yet played with the mod: installed only into a --game given
NO_MOD_WARNING = (
    "Without the mod (uninstalled, or removed by Steam or GOG when they verify, repair or update the "
    "game) the original game deletes the items that do not fit PC's bag. Before uninstalling, install "
    "PC's bag, then load and save each character.")
NO_MOD_WARNING_AFTER = (
    "A character not loaded and saved with PC's bag since keeps its items where the 360's bag put "
    "them, and without the mod the original game deletes those that do not fit PC's bag.")
BAG_WARNING = (
    "The 360's bag has one section of 50 slots for every kind of item; PC's has three of 21 (any "
    "item, spells, fish).\n\n"
    "When a character changes bag, the mod moves the items that do not fit to free slots or to the "
    "pet's bag; if some still do not fit, the game says so and closes without saving.\n\n"
    + NO_MOD_WARNING + "\n\nThe saves found are backed up first.\n\nUse the 360's bag?")


# ---- the PC game ----

def is_game(path):
    return bool(path) and os.path.isfile(os.path.join(path, "Torchlight.exe")) and (
        os.path.isfile(os.path.join(path, "CEGUIBase.dll")) or os.path.isfile(os.path.join(path, "CEGUIBase_orig.dll")))


def exe_timestamp(game):
    with open(os.path.join(game, "Torchlight.exe"), "rb") as f:
        head = f.read(4096)
    pe = struct.unpack_from("<I", head, 0x3C)[0]
    return struct.unpack_from("<I", head, pe + 8)[0]


def build_of(game):
    """The proxy's target for the game's Torchlight.exe (by its timestamp: build_proxy.TARGETS), or
    "unknown"."""
    try:
        stamp = exe_timestamp(game)
    except (OSError, struct.error):
        return "unknown"
    return next((t for t, (s, _) in build_proxy.TARGETS.items() if s == stamp), "unknown")


def _read_json(path):
    try:
        with open(path, encoding="utf-8") as f:
            return json.load(f)
    except (OSError, ValueError):
        return None


def steam_libraries(home):
    """Steam's steamapps folders: the usual roots and their libraryfolders.vdf entries."""
    roots = [os.path.join(home, p) for p in (".steam/steam", ".steam/debian-installation", ".local/share/Steam",
                                             ".var/app/com.valvesoftware.Steam/data/Steam")]
    if os.name == "nt":
        roots += [os.path.expandvars(r"%ProgramFiles(x86)%\Steam"), os.path.expandvars(r"%ProgramFiles%\Steam")]
    libs = []
    for root in roots:
        apps = os.path.join(root, "steamapps")
        if not os.path.isdir(apps):
            continue
        libs.append(apps)
        try:
            with open(os.path.join(apps, "libraryfolders.vdf"), encoding="utf-8", errors="ignore") as f:
                for m in re.finditer(r'"path"\s+"([^"]+)"', f.read()):
                    libs.append(os.path.join(m.group(1).replace("\\\\", "\\"), "steamapps"))
        except OSError:
            pass
    return list(dict.fromkeys(os.path.realpath(p) for p in libs))


def find_games(home=None):
    """[(store, path)] of the PC game's installs found (not checked for the build)."""
    home = home or os.path.expanduser("~")
    found = []

    def add(store, path):
        if is_game(path) and (store, os.path.realpath(path)) not in found:
            found.append((store, os.path.realpath(path)))

    # GOG: Heroic, Lutris, Wine prefixes, the usual folders.
    heroic = _read_json(os.path.join(home, ".config/heroic/gog_store/installed.json")) or {}
    for g in heroic.get("installed", []) if isinstance(heroic, dict) else []:
        add("gog", g.get("install_path"))
    for pattern in ("GOG Games/Torchlight", "Games/*/Torchlight", "Games/Heroic/Torchlight",
                    "*/drive_c/GOG Games/Torchlight", "*/*/drive_c/GOG Games/Torchlight",
                    "*/*/*/drive_c/GOG Games/Torchlight", ".wine/drive_c/GOG Games/Torchlight",
                    "Games/*/drive_c/GOG Games/Torchlight"):
        for p in glob.glob(os.path.join(home, pattern)):
            add("gog", p)
    if os.name == "nt":
        for p in (r"C:\GOG Games\Torchlight", os.path.expandvars(r"%ProgramFiles(x86)%\GOG Galaxy\Games\Torchlight")):
            add("gog", p)
        try:
            import winreg
            key = winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, r"SOFTWARE\WOW6432Node\GOG.com\Games")
            for i in range(winreg.QueryInfoKey(key)[0]):
                sub = winreg.OpenKey(key, winreg.EnumKey(key, i))
                try:
                    if "torchlight" in winreg.QueryValueEx(sub, "gameName")[0].lower():
                        add("gog", winreg.QueryValueEx(sub, "path")[0])
                except OSError:
                    pass
        except OSError:
            pass
    # Steam.
    for apps in steam_libraries(home):
        if os.path.isfile(os.path.join(apps, f"appmanifest_{STEAM_APP}.acf")):
            add("steam", os.path.join(apps, "common", "Torchlight"))
    # Epic: Legendary (and Heroic's copy of it), the launcher's manifests on Windows.
    for cfg in (".config/legendary/installed.json", ".config/heroic/legendaryConfig/legendary/installed.json"):
        data = _read_json(os.path.join(home, cfg)) or {}
        for g in data.values() if isinstance(data, dict) else []:
            if isinstance(g, dict) and "torchlight" in str(g.get("title", "")).lower():
                add("epic", g.get("install_path"))
    if os.name == "nt":
        for item in glob.glob(os.path.expandvars(r"%ProgramData%\Epic\EpicGamesLauncher\Data\Manifests\*.item")):
            g = _read_json(item) or {}
            if "torchlight" in str(g.get("DisplayName", "")).lower():
                add("epic", g.get("InstallLocation"))
    return found




# ---- the 360 copy ----

def is_x360_pak(path):
    try:
        with zipfile.ZipFile(path) as z:
            return any(n.lower() == X360_LAYOUT for n in z.namelist())
    except (OSError, zipfile.BadZipFile):
        return False


def is_x360_image(data):
    """A decrypted 360 image of Torchlight: a PE whose texts the converter reads at its addresses."""
    return data[:2] == b"MZ" and len(convert_x360_ui.image_strings(data)) >= len(convert_x360_ui.STRINGS) // 2


def check_package(pkg):
    """The user's message when the package is not Torchlight's Arcade one, else None."""
    if pkg.title_id != X360_TITLE:
        return f"This package is of another game (title {pkg.title_id:08X}), not Torchlight's ({X360_TITLE:08X})."
    if pkg.content_type != X360_CONTENT_TYPE:
        return f"This package is not the game itself (content type {pkg.content_type:08X})."
    return None


def read_360_copy(source, work):
    """(pak.zip, decrypted image) of the 360 copy into the folder work, from its package or an
    extracted folder; raises SystemExit with the user's message when they are not there."""
    pak, xex, image = os.path.join(work, "pak.zip"), os.path.join(work, "default.xex"), None
    if os.path.isfile(source):
        with open(source, "rb") as f:
            if f.read(4) not in stfs.MAGICS:
                source = os.path.dirname(source)   # a file of an extracted folder: the folder
    if os.path.isfile(source):
        try:
            pkg = stfs.Package(source)
        except (OSError, ValueError) as e:
            raise SystemExit(f"Cannot read {source} as an Xbox 360 package: {e}")
        with pkg:
            problem = check_package(pkg)
            if problem:
                raise SystemExit(problem)
            for name, dest in (("pak.zip", pak), ("default.xex", xex)):
                entry = pkg.find(name)
                if not entry:
                    raise SystemExit(f"The package has no {name}.")
                try:
                    with open(dest, "wb") as f:
                        pkg.read(entry, f)
                except ValueError as e:
                    raise SystemExit(f"Cannot read {name} from the package: {e}")
    elif os.path.isdir(source):
        found = {}
        for dirpath, _, files in os.walk(source):
            for name in files:
                low = name.lower()
                if low in ("pak.zip", "default.xex", "default.bin") and low not in found:
                    found[low] = os.path.join(dirpath, name)
        if "pak.zip" not in found:
            raise SystemExit(f"No pak.zip in {source}.")
        shutil.copyfile(found["pak.zip"], pak)
        if "default.bin" in found:
            image = found["default.bin"]
        elif "default.xex" in found:
            shutil.copyfile(found["default.xex"], xex)
        else:
            raise SystemExit(f"No default.xex in {source}.")
    else:
        raise SystemExit(f"{source} does not exist.")
    if not is_x360_pak(pak):
        raise SystemExit("The pak.zip found is not the Xbox 360 game's.")
    if image is None:
        import xex_dump
        print("decrypting the 360's default.xex (about 20 s without pycryptodome)")
        with open(xex, "rb") as f:
            try:
                _, data = xex_dump.dump(f.read())
            except Exception as e:   # the decompressor's own errors too: a damaged file
                raise SystemExit(f"Cannot decrypt default.xex: {e}")
        image = os.path.join(work, "default.bin")
        with open(image, "wb") as f:
            f.write(data)
    with open(image, "rb") as f:
        if not is_x360_image(f.read()):
            raise SystemExit("The 360's executable is not Torchlight's Arcade build 1.0.140.0.")
    return pak, image


# ---- saves ----

def save_folders(game, home=None):
    """The game's save folders: Windows' %APPDATA%; under Wine or Proton, the prefix the game is in
    and the prefixes beside it (a Proton run's own prefix next to the install's: "compat/pfx" by
    "prefix"), not every prefix in the home."""
    tail = os.path.join("AppData", "Roaming", "runic games", "torchlight", "save")
    home = home or os.path.expanduser("~")
    found = []
    if os.name == "nt" and os.environ.get("APPDATA"):
        found.append(os.path.join(os.environ["APPDATA"], "runic games", "torchlight", "save"))
    up = os.path.realpath(game)
    while os.path.dirname(up) != up and os.path.basename(up) != "drive_c":
        up = os.path.dirname(up)
    roots = []
    if os.path.basename(up) == "drive_c":
        roots.append(up)
        beside = os.path.dirname(os.path.dirname(up))          # the folder holding the prefix
        # Not when that is the home or above it (~/.wine: every prefix of the home, the review).
        if not os.path.realpath(home).startswith(os.path.realpath(beside)):
            for depth in ("*", "*/*"):
                roots += glob.glob(os.path.join(beside, depth, "drive_c"))
    for root in roots:
        found += glob.glob(os.path.join(root, "users", "*", tail))
    return [p for p in dict.fromkeys(os.path.realpath(p) for p in found) if os.path.isdir(p)
            and "compatdata" not in p.split(os.sep)]


def back_up_saves(folders):
    """A copy of each folder next to it, named by the time; their paths."""
    stamp = datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
    copies = []
    for folder in folders:
        copy = f"{folder}-backup-{stamp}"
        shutil.copytree(folder, copy)
        copies.append(copy)
    return copies


# ---- asking ----

class Ask:
    """Questions in windows when tkinter has a display, else on the console."""

    def __init__(self, console=False, yes=False):
        self.yes, self.tk = yes, None
        if not console and not yes:
            try:
                import tkinter
                from tkinter import filedialog, messagebox
                root = tkinter.Tk()
                root.withdraw()
                self.tk, self.filedialog, self.messagebox = root, filedialog, messagebox
            except Exception:   # no tkinter or no display
                self.tk = None

    def file(self, title):
        if self.yes:
            return None
        if self.tk:
            return self.filedialog.askopenfilename(title=title) or None
        try:
            return input(f"{title}: ").strip().strip('"') or None
        except EOFError:
            return None

    def yes_no(self, text, default=False):
        if self.yes:
            return default
        if self.tk:
            return self.messagebox.askyesno("Torchlight: the Xbox 360 interface", text,
                                            default="yes" if default else "no")
        try:
            answer = input(f"{text} [{'Y/n' if default else 'y/N'}] ").strip().lower()
        except EOFError:
            return default
        return default if not answer else answer.startswith("y")

    def info(self, text):
        print(text)
        if self.tk:
            self.messagebox.showinfo("Torchlight: the Xbox 360 interface", text)


# ---- install ----

def wine_path(path):
    """The game's own path for the ini: Windows' as it is; under Wine, Z: and backslashes."""
    return path if os.name == "nt" else "Z:" + os.path.abspath(path).replace("/", "\\")


def read_ini(game):
    path = os.path.join(game, "x360_ui.ini")
    values = {}
    if os.path.isfile(path):
        with open(path, encoding="utf-8", errors="ignore") as f:
            for line in f:
                if "=" in line:
                    k, v = line.rstrip("\r\n").split("=", 1)
                    values[k.strip()] = v.strip()
    return values


# The proxy's test options (x360_ui.ini, for runs on copies): never kept by an install; the proxy
# also warns on screen when it finds one. The same list in installer/src/install.cpp (ini_text) and
# proxy/cegui_proxy.c (read_config).
TEST_OPTIONS = ("save_root", "hold_saves", "pad_detect", "console")


def is_test_option(key):
    return key in TEST_OPTIONS or key.startswith("test_")


def mod_data_folders(game):
    """The proxy's own folder, %LOCALAPPDATA%\\x360ui (x360_stop.txt): beside each save folder's AppData
    (under Wine, the prefix's), and Windows' own."""
    found = []
    for folder in save_folders(game):   # .../AppData/Roaming/runic games/torchlight/save
        appdata = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(folder))))
        found.append(os.path.join(appdata, "Local", "x360ui"))
    if os.name == "nt" and os.environ.get("LOCALAPPDATA"):
        found.append(os.path.join(os.environ["LOCALAPPDATA"], "x360ui"))
    return [f for f in dict.fromkeys(found) if os.path.isdir(f)]


def write_ini(game, out, bag="pc"):
    """x360_ui.ini with the keys the installer owns set; other lines kept."""
    path = os.path.join(game, "x360_ui.ini")
    old = read_ini(game)
    # Remembered once the 360's bag was used, until uninstall: a character not yet loaded and saved
    # with PC's bag would lose what does not fit it.
    used = bag == "360" or old.get("bag") == "360" or old.get("bag_360_used") == "1"
    owned = {"mode": "scheme", "out": wine_path(out), "pad": "1", "pad_mode": "direct", "prompts": "auto",
             "bag": bag, "bag_360_used": "1" if used else None, "update_check": None}
    lines = []
    if os.path.isfile(path):
        with open(path, encoding="utf-8", errors="ignore") as f:
            lines = [l.rstrip("\r\n") for l in f
                     if l.split("=", 1)[0].strip() not in owned and not is_test_option(l.split("=", 1)[0].strip())]
    with open(path, "w", encoding="utf-8", newline="\r\n") as f:
        for k, v in owned.items():
            if v is not None:
                f.write(f"{k}={v}\n")
        for l in lines:
            if l.strip():
                f.write(l + "\n")


def converted_folders(game):
    out = os.path.join(game, "x360ui")
    return [out] + [f"{out}-{style}" for style in STYLES]


def convert_all(pak, image, pc_pak, game):
    """x360ui and x360ui-STYLE in the game's folder, each made in a .partial folder first."""
    script = os.path.join(HERE, "convert_x360_ui.py")
    for style, dest in zip(("xbox",) + STYLES, converted_folders(game)):
        partial = dest + ".partial"
        shutil.rmtree(partial, ignore_errors=True)
        cmd = [sys.executable, script, pak, partial, "--image", image, "--pc-pak", pc_pak]
        if style == "nintendo-swapped":
            cmd += ["--prompts", "nintendo", "--swap-confirm-cancel"]   # Nintendo's layout (the proxy's setting)
        elif style != "xbox":
            cmd += ["--prompts", style]
        print(f"converting the 360's interface ({style})")
        subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL)
        shutil.rmtree(dest, ignore_errors=True)
        os.rename(partial, dest)
    return converted_folders(game)[0]


def proxy_dll(build, game, given=None):
    """The proxy to install: given, a release's one next to this script, or one built from this
    repository (clang-cl and lld-link) when it is here."""
    if given:
        return given
    near = os.path.join(HERE, PROXY_FILES.get(build, ""))
    if PROXY_FILES.get(build) and os.path.isfile(near) and build_proxy.is_proxy(near):
        return near
    real = os.path.join(game, "CEGUIBase_orig.dll")
    if not os.path.isfile(real):
        real = os.path.join(game, "CEGUIBase.dll")
    out = tempfile.mkdtemp(prefix="x360ui-build-", dir=game)
    build_proxy.build(real, out, build)
    return os.path.join(out, "CEGUIBase.dll")


def check_real_dll(game, exports_list=cegui_exports.LIST):
    """SystemExit unless the game's real CEGUIBase.dll exports what the proxy forwards to
    (proxy/cegui_exports.txt): another DLL would leave the game calling into nothing."""
    dll, orig = os.path.join(game, "CEGUIBase.dll"), os.path.join(game, "CEGUIBase_orig.dll")
    real = dll if os.path.isfile(dll) and not build_proxy.is_proxy(dll) else orig
    if not os.path.isfile(real) or build_proxy.is_proxy(real):
        raise SystemExit(f"{game}: the real CEGUIBase.dll is missing: reinstall the game's files.")
    diff = cegui_exports.differences(real, exports_list)
    if diff:
        raise SystemExit(f"{real} is not the CEGUIBase.dll this mod is made for ({len(diff)} exports differ).")


def has_mod(game):
    return os.path.isfile(os.path.join(game, "x360_ui.ini")) or os.path.isfile(os.path.join(game, "CEGUIBase_orig.dll"))


def pick_game(given, ask=None, installed_only=False):
    if given:
        game = os.path.realpath(given)
        if not is_game(game):
            raise SystemExit(f"No Torchlight.exe and CEGUIBase.dll in {game}.")
        return game, build_of(game)
    games = find_games()
    supported = [(s, p) for s, p in games if build_of(p) in PROXY_FILES and build_of(p) not in EXPERIMENTAL]
    for store, path in games:
        if build_of(path) in EXPERIMENTAL:
            print(f"{store}: {path}: its build is experimental (not yet played with the mod); "
                  f"give it with --game to install there.")
    for store, path in games:
        if build_of(path) not in PROXY_FILES:
            print(f"{store}: {path}: a build this version does not support yet, left alone.")
    if not supported:
        raise SystemExit("No supported PC Torchlight found; give its folder with --game.")
    if installed_only and any(has_mod(p) for _, p in supported):   # uninstall: only those with the mod
        supported = [(s, p) for s, p in supported if has_mod(p)]
    if len(supported) > 1:
        # More than one: each shown, and the player says which (--yes: the first, said so).
        found = f"{len(supported)} supported PC Torchlights were found:\n" + "\n".join(f"{s}: {p}" for s, p in supported)
        if not ask or ask.yes:
            print(found + f"\nusing {supported[0][0]}: {supported[0][1]} (--game picks another)")
            return supported[0][1], build_of(supported[0][1])
        for i, (store, path) in enumerate(supported):
            if ask.yes_no(found + f"\n\nUse {store}: {path}?", default=i == 0):
                return path, build_of(path)
        raise SystemExit("No game chosen; give its folder with --game.")
    return supported[0][1], build_of(supported[0][1])


def do_install(args):
    ask = Ask(args.get("console"), args.get("yes"))
    game, build = pick_game(args.get("game"), ask)
    if build not in PROXY_FILES:
        raise SystemExit(f"{game}: a build this version does not support yet.")
    print(f"the game: {game} ({build} build)")
    check_real_dll(game)
    if build in EXPERIMENTAL and not (args.get("experimental") or Ask(args.get("console")).yes_no(
            f"This game's build ({build}) is experimental: the mod was ported to it offline "
            "and has not been played on it yet. Install anyway?", default=False)):
        print("not installed (an experimental build needs a yes, or --experimental)")
        return 1
    source = args.get("package") or args.get("x360_dir") or ask.file(
        "Your Torchlight Xbox LIVE Arcade package (or a file in a folder extracted from it)")
    if not source:
        raise SystemExit("No 360 package given (--package FILE or --x360-dir DIR).")
    bag = args.get("bag") or ("360" if ask.yes_no(BAG_WARNING, default=False) else "pc")
    # The work folder in the game's folder: the package's pak.zip and the decrypted image are large
    # for a /tmp in memory (a tmpfs with a quota filled up in the first try).
    with tempfile.TemporaryDirectory(prefix="x360ui-work-", dir=game) as work:
        pak, image = read_360_copy(source, work)
        print(f"the 360 copy: {source}")
        backups = []
        if bag == "360":
            folders = save_folders(game)
            if not folders and not ask.yes_no(
                    "No save folder of this game was found to back up (a Steam install's saves are in "
                    "Steam's prefix, left alone; Heroic's prefix may be elsewhere). Back up your saves "
                    "yourself first.\n\nUse the 360's bag without a backup?", default=False):
                bag = "pc"
                print("PC's bag kept")
            backups = back_up_saves(folders)
            for b in backups:
                print(f"saves backed up: {b}")
        out = convert_all(pak, image, os.path.join(game, "Pak.zip"), game)
    write_ini(game, out, bag)
    proxy = proxy_dll(build, game, args.get("dll"))
    build_proxy.install(game, proxy)
    built = os.path.dirname(proxy)
    if os.path.basename(built).startswith("x360ui-build-") and os.path.dirname(built) == game:
        shutil.rmtree(built)                    # the proxy built here is in place: its folder not kept
    ask.info("Installed. Start Torchlight with a pad connected."
             + (f"\n\nYour saves were backed up:\n" + "\n".join(backups) if backups else ""))
    return 0


def do_uninstall(args):
    ask = Ask(args.get("console"), args.get("yes"))
    game, _ = pick_game(args.get("game"), ask, installed_only=True)
    backups = []
    if read_ini(game).get("bag", "pc") == "360":
        if not args.get("anyway") and ask.yes_no(
                "The 360's bag is installed. " + NO_MOD_WARNING + "\n\nSwitch to PC's bag now (recommended)? "
                "Then load and save each character, and uninstall.", default=True):
            write_ini(game, converted_folders(game)[0], "pc")
            ask.info("PC's bag installed; nothing uninstalled. Load and save each character, then uninstall.")
            return 0
        if not args.get("anyway") and not ask.yes_no(
                "Uninstall anyway? The original game will delete the items that do not fit PC's bag. "
                "Your saves are backed up first.", default=False):
            print("not uninstalled")
            return 1
        backups = back_up_saves(save_folders(game))
        for b in backups:
            print(f"saves backed up: {b}")
    elif read_ini(game).get("bag_360_used") == "1":
        if not args.get("anyway") and not ask.yes_no(
                "PC's bag is installed, and the 360's was used before. " + NO_MOD_WARNING_AFTER +
                "\n\nHave you loaded and saved each character with PC's bag? Your saves are backed up first.",
                default=False):
            print("not uninstalled (load and save each character with PC's bag first)")
            return 1
        backups = back_up_saves(save_folders(game))
        for b in backups:
            print(f"saves backed up: {b}")
    if not build_proxy.remove(game):
        raise SystemExit("CEGUIBase.dll is not the proxy nor the real one; nothing removed")
    for folder in converted_folders(game) + glob.glob(os.path.join(game, "x360ui-build-*")) + glob.glob(
            os.path.join(game, "x360ui-work-*")):
        shutil.rmtree(folder, ignore_errors=True)
        shutil.rmtree(folder + ".partial", ignore_errors=True)
    for name in ["x360_ui.ini", "x360_ui.log", "x360_update.txt", "x360_settings.ini", "x360_stop.txt"] + [
            os.path.basename(p) for p in glob.glob(os.path.join(game, "x360_dump_*.json"))]:
        try:
            os.remove(os.path.join(game, name))
        except OSError:
            pass
    for folder in mod_data_folders(game):
        shutil.rmtree(folder, ignore_errors=True)
    if os.name == "nt" and os.environ.get("LOCALAPPDATA"):
        # Files an older version wrote beside the game in Program Files, which Windows redirected
        # into the user's VirtualStore.
        store = os.path.join(os.environ["LOCALAPPDATA"], "VirtualStore",
                             os.path.splitdrive(os.path.abspath(game))[1].lstrip("\\/"))
        for name in ("x360_ui.log", "x360_settings.ini", "x360_stop.txt"):
            try:
                os.remove(os.path.join(store, name))
            except OSError:
                pass
    ask.info(f"Uninstalled from {game}." + (f"\n\nYour saves were backed up:\n" + "\n".join(backups) if backups else ""))
    return 0


def do_list(args):
    games = find_games()
    for store, path in games:
        print(f"{store}: {path} ({build_of(path)} build)")
    if not games:
        print("No PC Torchlight found.")
    return 0 if games else 1


def parse(argv):
    args, i = {"command": "install"}, 0
    if argv and argv[0] in ("install", "uninstall", "list"):
        args["command"], argv = argv[0], argv[1:]
    flags = {"--yes": "yes", "--console": "console", "--experimental": "experimental", "--anyway": "anyway"}
    values = {"--game": "game", "--package": "package", "--x360-dir": "x360_dir", "--bag": "bag", "--dll": "dll"}
    while i < len(argv):
        a = argv[i]
        if a in flags:
            args[flags[a]] = True
        elif a in values and i + 1 < len(argv):
            args[values[a]] = argv[i + 1]
            i += 1
        else:
            raise SystemExit(__doc__)
        i += 1
    if args.get("bag") not in (None, "pc", "360"):
        raise SystemExit("--bag is pc or 360")
    return args


def main(argv=None):
    args = parse(sys.argv[1:] if argv is None else argv)
    return {"install": do_install, "uninstall": do_uninstall, "list": do_list}[args["command"]](args)


if __name__ == "__main__":
    sys.exit(main())
