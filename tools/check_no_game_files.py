#!/usr/bin/env python3
"""Fail if a game file type is tracked by git (the repository holds no game data).

check_no_game_files.py [ROOT]: lists the files git tracks under ROOT (default: the current
directory) and exits with 1, naming them, if any has a forbidden extension.

The one exception is EXPORT_LIST, CEGUIBase.dll's exported names (tools/cegui_exports.py): it must
hold only lines of that format, each a mangled name in CEGUI's namespace, and no other tracked file
may hold such lines (nor be a .def or .lib, an export table's other forms).
"""
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cegui_exports  # noqa: E402

FORBIDDEN = (
    ".pak", ".dds", ".layout", ".imageset", ".exe", ".dll",
    ".uilayout", ".scheme", ".looknfeel", ".font", ".ttf", ".xex", ".def", ".lib",
)
EXPORT_LIST = "proxy/cegui_exports.txt"
# A line of the list's format, wherever it is.
EXPORT_LINE = re.compile(rb"^[0-9]+ (cegui|runic) \?", re.M)


def forbidden(paths):
    # The games' data archives are `pak.zip` (360) and `Pak.zip` (PC).
    return [p for p in paths if p.lower().endswith(FORBIDDEN) or p.lower().rsplit("/", 1)[-1] == "pak.zip"]


def tracked(root):
    out = subprocess.run(["git", "-C", root, "ls-files", "-z"], check=True, capture_output=True).stdout
    return [p for p in out.decode("utf-8", "replace").split("\0") if p]


def export_problems(root, paths):
    """What is wrong about export tables: EXPORT_LIST malformed, or export lines elsewhere."""
    out = []
    for p in paths:
        full = os.path.join(root, p)
        if not os.path.isfile(full):
            continue
        with open(full, "rb") as f:
            data = f.read()
        if p == EXPORT_LIST:
            try:
                cegui_exports.parse(data.decode("ascii"), p)
            except (UnicodeDecodeError, ValueError) as e:
                out.append(f"{p}: {e}")
        elif EXPORT_LINE.search(data):
            out.append(f"{p}: export table lines outside {EXPORT_LIST}")
    return out


def main():
    root = sys.argv[1] if len(sys.argv) > 1 else "."
    paths = tracked(root)
    bad = forbidden(paths)
    for p in bad:
        print(f"forbidden game file type: {p}")
    problems = export_problems(root, paths)
    for p in problems:
        print(p)
    bad += problems
    if bad:
        print(f"{len(bad)} game file(s) tracked; this repository holds no game data.")
        return 1
    print("no game files tracked")
    return 0


if __name__ == "__main__":
    sys.exit(main())
