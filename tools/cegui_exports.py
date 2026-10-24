#!/usr/bin/env python3
"""The export table of Torchlight's CEGUIBase.dll kept as names only (proxy/cegui_exports.txt), so
the proxy builds without the game's DLL (CI, releases).

    cegui_exports.py make GAME_DLL OFFICIAL_DLL [OUT]
        the list from the game's DLL, each name marked "cegui" when the official CEGUI build
        (OFFICIAL_DLL, bin/CEGUIBase.dll of CEGUI-SDK-0.6.2b-vc9.zip) exports it, else "runic"
        (Runic's own additions); written to OUT (default: proxy/cegui_exports.txt)
    cegui_exports.py check DLL [LIST]
        exit 1 unless DLL exports exactly the list's names with the list's ordinals

The file holds only the exported (mangled C++) names of CEGUI's classes and their ordinals: no
code, data or text of the game. Every name must be in CEGUI's namespace (tools/check_no_game_files.py
checks it in CI).

A line: ORDINAL SOURCE NAME, ordered as the DLL's name table (by name). Up to 10 # lines of
header before the first; no other line (blank or comment) is accepted.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from pe_info import PE  # noqa: E402

LIST = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "proxy", "cegui_exports.txt")
SOURCES = ("cegui", "runic")
# An MSVC mangled name (printable ASCII) whose qualified name, the part before the first "@@",
# ends in namespace CEGUI: a member of a class there ("?f@Font@CEGUI@@...", templates' arguments
# included) or a free operator of CEGUI ("??8CEGUI@@...").
NAME = re.compile(r"^\?[!-~]+$")
SCOPE = re.compile(r"^(.*@CEGUI|\?\?_?[0-9A-Z]CEGUI)$")
LINE = re.compile(r"^([1-9][0-9]{0,4}) (cegui|runic) (\?[!-~]+)$")
MAX_COMMENTS = 10   # the header's lines, before the first entry; none after it


def in_cegui(name):
    return bool(NAME.match(name) and "@@" in name and SCOPE.match(name.split("@@", 1)[0]))


def dll_exports(path):
    with open(path, "rb") as f:
        return PE(f.read()).exports()


def parse(text, where="the list"):
    """[(name, ordinal, source)] of the list's text; ValueError on a malformed line."""
    out, names, ordinals, comments = [], set(), set(), 0
    for n, line in enumerate(text.splitlines(), 1):
        if line.startswith("#") and not out:
            comments += 1
            if comments > MAX_COMMENTS:
                raise ValueError(f"{where}:{n}: more than {MAX_COMMENTS} header lines")
            continue
        m = LINE.match(line)
        if not m or not in_cegui(m.group(3)):
            raise ValueError(f"{where}:{n}: not 'ORDINAL cegui|runic NAME' with a name in CEGUI's namespace")
        name, ordinal = m.group(3), int(m.group(1))
        if name in names or ordinal in ordinals:
            raise ValueError(f"{where}:{n}: {name} or ordinal {ordinal} twice")
        names.add(name)
        ordinals.add(ordinal)
        out.append((name, ordinal, m.group(2)))
    if not out:
        raise ValueError(f"{where}: no exports")
    return out


def read(path=LIST):
    with open(path, encoding="ascii") as f:
        return parse(f.read(), path)


def exports(path=LIST):
    """[(name, ordinal)] as PE.exports() gives them, for build_proxy.proxy_def."""
    return [(n, o) for n, o, _ in read(path)]


def make(game_dll, official_dll, out=LIST):
    game, official = dll_exports(game_dll), {n for n, _ in dll_exports(official_dll)}
    bad = [n for n, _ in game if not in_cegui(n)]
    if bad:
        raise SystemExit(f"{game_dll} exports names outside CEGUI's namespace: {bad[:5]}")
    runic = [n for n, _ in game if n not in official]
    lines = [
        "# The export table of Torchlight's CEGUIBase.dll (CEGUI 0.6.2 as built by Runic Games): names",
        "# and ordinals only, for building the proxy without the game's DLL. Made by",
        "# tools/cegui_exports.py make; where it comes from: docs/xbox-ui-on-pc.md, section 18.",
        f"# {len(game)} exports: {len(game) - len(runic)} cegui (exported by the official",
        "# CEGUI-SDK-0.6.2b-vc9 bin/CEGUIBase.dll), " f"{len(runic)} runic (only in the game's DLL).",
        "# ORDINAL SOURCE NAME",
    ]
    lines += [f"{o} {'runic' if n in runic else 'cegui'} {n}" for n, o in game]
    text = "\n".join(lines) + "\n"
    parse(text)
    with open(out, "w", encoding="ascii", newline="\n") as f:
        f.write(text)
    print(f"{out}: {len(game)} exports, {len(runic)} runic")


def differences(dll, path=LIST):
    """What differs between DLL's export table and the list ([] when the same)."""
    have, want = set(dll_exports(dll)), set(exports(path))
    return ([f"not in the list: {o} {n}" for n, o in sorted(have - want)]
            + [f"not exported by the DLL: {o} {n}" for n, o in sorted(want - have)])


def main(argv):
    if len(argv) >= 3 and argv[0] == "make" and len(argv) <= 4:
        make(*argv[1:])
        return 0
    if len(argv) >= 2 and argv[0] == "check" and len(argv) <= 3:
        diff = differences(*argv[1:])
        for d in diff[:20]:
            print(d)
        print(f"{len(diff)} differences" if diff else "same exports")
        return 1 if diff else 0
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
