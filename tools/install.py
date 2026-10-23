#!/usr/bin/env python3
"""Installs the Xbox 360 interface into the user's PC Torchlight, from the user's own 360 copy.

    install.py [--game DIR] [--x360-dir DIR] [--x360-image FILE] [--dll FILE] [--list] [--remove]

What it does:
1. Finds the PC game: GOG (Heroic's, Lutris's or any Wine prefix's "drive_c/GOG Games/Torchlight",
   GOG Galaxy's registry entry on Windows, the usual folders), Steam (its library folders,
   appmanifest_41500.acf) and the Epic Games Store (Legendary's and Heroic's installed.json, the
   launcher's manifests on Windows). --list prints what it found and stops.
2. Checks the build: only GOG's (Torchlight.exe of build 17870, its PE timestamp 0x4ffc6815) is
   supported; the proxy's addresses are that build's. Steam's and Epic's are reported and left alone.
3. Finds the user's 360 copy: an extracted XBLA package (pak.zip with the 360's layouts, and
   default.xex) and its decrypted image (default.bin, as XEX tools write it). Without a decrypted
   image, 360tools' extract_pe.py is used when it is installed (--xex-tool); else the steps to make
   one are printed. Nothing of either game is copied into this repository or sent anywhere.
4. Converts the 360's interface into the game's folder: x360ui (Xbox prompts) and x360ui-STYLE for
   playstation3, playstation4, playstation5 and nintendo (the proxy picks one by the connected pad
   at start, proxy/setup.c), each with the 360's texts (--image) and PC's tips (--pc-pak).
5. Writes x360_ui.ini (the keys it owns; others kept) and installs the proxy CEGUIBase.dll, the
   real one renamed CEGUIBase_orig.dll (tools/build_proxy.py install). The proxy is --dll, a
   CEGUIBase.dll next to this script (a release's), or built with clang-cl when the repo is here.

--remove puts the real CEGUIBase.dll back and leaves the converted folders.
"""
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
import build_proxy  # noqa: E402
import convert_x360_ui  # noqa: E402

GOG_TIMESTAMP = 0x4FFC6815        # Torchlight.exe of GOG's build 17870, the one the proxy knows
STEAM_APP = "41500"
STYLES = ("playstation3", "playstation4", "playstation5", "nintendo")
X360_LAYOUT = "media/ui/mastermenu_xenon.layout"     # a file only the 360's pak has


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
    """"gog" for the build the proxy knows, else "unknown"."""
    try:
        return "gog" if exe_timestamp(game) == GOG_TIMESTAMP else "unknown"
    except (OSError, struct.error):
        return "unknown"


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


def is_x360_image(path):
    """A decrypted 360 image of Torchlight: a PE whose texts the converter reads at its addresses."""
    try:
        if not 8_000_000 < os.path.getsize(path) < 64_000_000:
            return False
        with open(path, "rb") as f:
            data = f.read()
    except OSError:
        return False
    return data[:2] == b"MZ" and len(convert_x360_ui.image_strings(data)) >= len(convert_x360_ui.STRINGS) // 2


def find_x360(home=None, roots=None, depth=6):
    """(pak.zip, decrypted image or None, default.xex or None) of the 360 copy found under the
    roots (the home and mounted media by default), or (None, None, None)."""
    home = home or os.path.expanduser("~")
    roots = roots or [home, "/media", "/mnt", "/run/media"]
    paks, images = [], []
    for root in roots:
        base = root.rstrip(os.sep).count(os.sep)
        for dirpath, dirnames, filenames in os.walk(root):
            if dirpath.count(os.sep) - base >= depth:
                dirnames[:] = []
            dirnames[:] = [d for d in dirnames if not d.startswith(".") and d not in ("node_modules", "proc", "sys")]
            for name in filenames:
                low = name.lower()
                p = os.path.join(dirpath, name)
                if low == "pak.zip" and is_x360_pak(p):
                    paks.append(p)
                elif low.endswith((".bin", ".pe")) and low.startswith("default") and is_x360_image(p):
                    images.append(p)
    if not paks:
        return None, None, None
    pak = paks[0]
    xex = os.path.join(os.path.dirname(pak), "default.xex")
    near = [i for i in images if os.path.dirname(i) == os.path.dirname(pak)]
    return pak, (near or images or [None])[0], xex if os.path.isfile(xex) else None


def decrypt_xex(xex, tool, out):
    """The decrypted image of default.xex by 360tools' extract_pe.py (the user's tool, its own
    dependencies), or None."""
    if not tool or not os.path.isfile(tool):
        return None
    r = subprocess.run([sys.executable, tool, xex, out], cwd=os.path.dirname(tool), capture_output=True, text=True)
    return out if r.returncode == 0 and is_x360_image(out) else None


# ---- install ----

def wine_path(path):
    """The game's own path for the ini: Windows' as it is; under Wine, Z: and backslashes."""
    return path if os.name == "nt" else "Z:" + os.path.abspath(path).replace("/", "\\")


def write_ini(game, out):
    """x360_ui.ini with the keys the installer owns set; other lines kept."""
    path = os.path.join(game, "x360_ui.ini")
    owned = {"mode": "scheme", "out": wine_path(out), "pad": "1", "pad_mode": "direct", "prompts": "auto"}
    lines = []
    if os.path.isfile(path):
        with open(path, encoding="utf-8", errors="ignore") as f:
            lines = [l.rstrip("\r\n") for l in f if l.split("=", 1)[0].strip() not in owned]
    with open(path, "w", encoding="utf-8", newline="\r\n") as f:
        for k, v in owned.items():
            f.write(f"{k}={v}\n")
        for l in lines:
            if l.strip():
                f.write(l + "\n")


def convert_all(pak, image, pc_pak, game):
    """x360ui and x360ui-STYLE in the game's folder."""
    out = os.path.join(game, "x360ui")
    script = os.path.join(HERE, "convert_x360_ui.py")
    for style in ("xbox",) + STYLES:
        dest = out if style == "xbox" else f"{out}-{style}"
        cmd = [sys.executable, script, pak, dest, "--image", image, "--pc-pak", pc_pak]
        if style != "xbox":
            cmd += ["--prompts", style]
        print(f"converting the 360's interface ({style}) into {dest}")
        subprocess.run(cmd, check=True)
    return out


def proxy_dll(args, game):
    """The proxy to install: --dll, one next to this script, or one built from this repo."""
    if args.get("dll"):
        return args["dll"]
    near = os.path.join(HERE, "CEGUIBase.dll")
    if os.path.isfile(near) and build_proxy.is_proxy(near):
        return near
    real = os.path.join(game, "CEGUIBase_orig.dll")
    if not os.path.isfile(real):
        real = os.path.join(game, "CEGUIBase.dll")
    out = tempfile.mkdtemp(prefix="x360ui-build-")
    build_proxy.build(real, out)
    return os.path.join(out, "CEGUIBase.dll")


def parse(argv):
    args, i = {}, 0
    flags = {"--list": "list", "--remove": "remove"}
    values = {"--game": "game", "--x360-dir": "x360_dir", "--x360-image": "image", "--dll": "dll", "--xex-tool": "xex_tool"}
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
    return args


def main(argv=None):
    args = parse(sys.argv[1:] if argv is None else argv)
    games = [("given", os.path.realpath(args["game"]))] if args.get("game") else find_games()
    if args.get("list") or not games:
        for store, path in games:
            print(f"{store}: {path} ({build_of(path)} build)")
        if not games:
            print("No PC Torchlight found; give its folder with --game.")
        return 0 if games else 1
    supported = [(s, p) for s, p in games if build_of(p) == "gog"]
    for store, path in games:
        if build_of(path) != "gog":
            print(f"{store}: {path}: not GOG's build 17870; only that one is supported for now, left alone.")
    if not supported:
        return 1
    game = supported[0][1]
    print(f"the game: {game}")
    if args.get("remove"):
        build_proxy.remove(game)
        return 0
    if args.get("x360_dir"):
        pak, image, xex = find_x360(roots=[args["x360_dir"]], depth=3)
    else:
        pak, image, xex = find_x360()
    image = args.get("image") or image
    if not pak:
        print("No 360 copy found (its extracted pak.zip with the 360's layouts); give its folder with --x360-dir.")
        return 1
    if not image and xex:
        tool = args.get("xex_tool") or os.path.expanduser("~/360tools/tools/extract_pe.py")
        image = decrypt_xex(xex, tool, os.path.join(tempfile.mkdtemp(prefix="x360ui-xex-"), "default.bin"))
    if not image:
        print(f"The 360's {xex or 'default.xex'} is not decrypted. Decrypt it into default.bin (360tools' "
              "extract_pe.py, with pycryptodome; or any XEX tool) and give it with --x360-image.")
        return 1
    print(f"the 360 copy: {pak}\nits image: {image}")
    out = convert_all(pak, image, os.path.join(game, "Pak.zip"), game)
    write_ini(game, out)
    build_proxy.install(game, proxy_dll(args, game))
    print("done: start Torchlight with a pad.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
