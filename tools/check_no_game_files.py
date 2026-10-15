#!/usr/bin/env python3
"""Fail if a game file type is tracked by git (the repository holds no game data).

check_no_game_files.py [ROOT]: lists the files git tracks under ROOT (default: the current
directory) and exits with 1, naming them, if any has a forbidden extension.
"""
import subprocess
import sys

FORBIDDEN = (
    ".pak", ".dds", ".layout", ".imageset", ".exe", ".dll",
    ".uilayout", ".scheme", ".looknfeel", ".font", ".ttf", ".xex",
)


def forbidden(paths):
    # The games' data archives are `pak.zip` (360) and `Pak.zip` (PC).
    return [p for p in paths if p.lower().endswith(FORBIDDEN) or p.lower().rsplit("/", 1)[-1] == "pak.zip"]


def tracked(root):
    out = subprocess.run(["git", "-C", root, "ls-files", "-z"], check=True, capture_output=True).stdout
    return [p for p in out.decode("utf-8", "replace").split("\0") if p]


def main():
    root = sys.argv[1] if len(sys.argv) > 1 else "."
    bad = forbidden(tracked(root))
    for p in bad:
        print(f"forbidden game file type: {p}")
    if bad:
        print(f"{len(bad)} game file(s) tracked; this repository holds no game data.")
        return 1
    print("no game files tracked")
    return 0


if __name__ == "__main__":
    sys.exit(main())
