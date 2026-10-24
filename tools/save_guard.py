#!/usr/bin/env python3
"""Back up the saves a GOG run will really use, and check the copy, before the run.

save_guard.py where COMPAT                           the save folder the game will use in COMPAT
save_guard.py backup COMPAT BACKUPS [--search ROOT]  copy it to BACKUPS/run-<time>/save, checked

COMPAT is the Proton prefix `tools/run_gog.sh` runs in (its `pfx` holds Wine's tree). The save
folder is found the way the game finds it: Wine's `AppData` folder, read from the prefix's
`user.reg`, then `runic games/torchlight/save` (Wine's paths are case-insensitive). `backup`
hashes the folder, copies it, hashes the copy and the folder again, and exits 1 if any hash
differs, if the game is running, or if the folder found has no save while another folder under
ROOT has some (the first prefix's folder was once backed up in place of the one the game used;
docs/regressions.md). The saves themselves are only read.
"""
import argparse
import hashlib
import os
import shutil
import subprocess
import sys
import time

SAVE_PARTS = ("runic games", "torchlight", "save")
SHELL_FOLDERS = "[Software\\\\Microsoft\\\\Windows\\\\CurrentVersion\\\\Explorer\\\\Shell Folders]"


class GuardError(Exception):
    pass


def appdata_from_user_reg(text):
    """The `AppData` value of Shell Folders in a Wine `user.reg`, as a Windows path."""
    in_section = False
    for line in text.splitlines():
        if line.startswith("["):
            in_section = line.split("]")[0] + "]" == SHELL_FOLDERS
        elif in_section and line.startswith('"AppData"='):
            return line.split("=", 1)[1].strip().strip('"').replace("\\\\", "\\")
    return None


def find_ci(base, parts):
    """base/parts..., each part matched without regard to case, as Wine does; None if missing."""
    path = base
    for part in parts:
        if not os.path.isdir(path):
            return None
        names = [n for n in os.listdir(path) if n.lower() == part.lower()]
        if not names:
            return None
        path = os.path.join(path, sorted(names)[0])
    return path


def save_folder(compat):
    """(the save folder the game will use, or None if not there yet; the folder it would be)."""
    if "compatdata/41500" in os.path.abspath(compat).replace(os.sep, "/"):
        raise GuardError("refusing: that is Steam's Torchlight prefix")
    pfx = os.path.join(compat, "pfx")
    reg = os.path.join(pfx, "user.reg")
    if not os.path.isdir(pfx):
        return None, os.path.join(pfx, "(no prefix yet)")
    if not os.path.isfile(reg):
        raise GuardError(f"{reg} is missing: cannot tell where the game saves")
    with open(reg, encoding="utf-8", errors="replace") as f:
        appdata = appdata_from_user_reg(f.read())
    if not appdata or len(appdata) < 3 or appdata[1:3] != ":\\":
        raise GuardError(f"no AppData folder in {reg}")
    parts = ["drive_" + appdata[0].lower()] + [p for p in appdata[3:].split("\\") if p]
    expected = os.path.join(pfx, *parts, *SAVE_PARTS)
    return find_ci(pfx, parts + list(SAVE_PARTS)), expected


def hashes(folder):
    """{relative path: SHA-256} of every file under folder."""
    out = {}
    for d, _dirs, files in os.walk(folder):
        for name in files:
            p = os.path.join(d, name)
            h = hashlib.sha256()
            with open(p, "rb") as f:
                for block in iter(lambda: f.read(1 << 20), b""):
                    h.update(block)
            out[os.path.relpath(p, folder)] = h.hexdigest()
    return out


def has_saves(folder):
    return bool(folder) and os.path.isdir(folder) and any(
        n.lower().endswith(".svt") for n in os.listdir(folder))


def other_save_folders(root, skip):
    """Save folders with saves under root, other than skip."""
    found = []
    for d, dirs, _files in os.walk(root):
        dirs[:] = [n for n in dirs if not n.lower().startswith("save-backup")]
        if [p.lower() for p in d.replace(os.sep, "/").split("/")[-3:]] == list(SAVE_PARTS):
            if has_saves(d) and (not skip or os.path.realpath(d) != os.path.realpath(skip)):
                found.append(d)
            dirs[:] = []
    return sorted(found)


def game_running():
    try:
        return subprocess.run(["pgrep", "-f", "Torchlight.exe"], capture_output=True).returncode == 0
    except FileNotFoundError:
        return False


def backup(compat, backups, search=None, stamp=None, running=game_running):
    """Copy the save folder to backups/run-<stamp>/save; the copy's folder, or None if no saves."""
    if running():
        raise GuardError("Torchlight.exe is running: it may write the saves during the copy")
    folder, expected = save_folder(compat)
    others = other_save_folders(search, folder) if search else []
    if not has_saves(folder):
        if others:
            raise GuardError(f"the game will use {expected}, which has no save, but these have "
                             f"some: {', '.join(others)}; check the prefix before running")
        print(f"no saves yet in {expected}: nothing to back up")
        return None
    before = hashes(folder)
    out = os.path.join(backups, "run-" + (stamp or time.strftime("%Y%m%d-%H%M%S")))
    if os.path.exists(out):
        raise GuardError(f"{out} already exists")
    shutil.copytree(folder, os.path.join(out, "save"))
    copy, after = hashes(os.path.join(out, "save")), hashes(folder)
    if copy != before or after != before:
        raise GuardError(f"the copy in {out} does not match the saves: do not run")
    with open(os.path.join(out, "SHA256SUMS"), "w") as f:
        for rel in sorted(before):
            f.write(f"{before[rel]}  {rel}\n")
    print(f"saves of {folder}: {len(before)} file(s) backed up to {out}, hashes checked")
    for other in others:
        print(f"note: not used by this run: {other}")
    return out


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    sub = ap.add_subparsers(dest="cmd", required=True)
    w = sub.add_parser("where")
    w.add_argument("compat")
    b = sub.add_parser("backup")
    b.add_argument("compat")
    b.add_argument("backups")
    b.add_argument("--search", help="a folder whose other save folders are checked too")
    args = ap.parse_args(argv)
    try:
        if args.cmd == "where":
            folder, expected = save_folder(args.compat)
            print(folder or f"{expected} (not there yet)")
        else:
            backup(args.compat, args.backups, args.search)
    except GuardError as e:
        print(f"save_guard: {e}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
