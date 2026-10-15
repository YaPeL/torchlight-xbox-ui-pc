#!/usr/bin/env python3
"""The named windows of CEGUI layouts (PC `.layout`, 360 `.uilayout`), and how two layouts overlap.

layout_windows.py A                 the named windows of A (name, type, parent)
layout_windows.py A B               of A's named windows, which B has too
A layout is read from the user's copy (a file extracted from the game's pak); nothing is written.
"""
import collections
import re
import sys

WINDOW = re.compile(r"<Window\b([^>]*?)(/?)>|</Window\s*>", re.S)
ATTR = re.compile(r"(\w+)\s*=\s*\"([^\"]*)\"")


def text_of(data):
    if data[:2] in (b"\xff\xfe", b"\xfe\xff"):
        return data.decode("utf-16")
    if data[:3] == b"\xef\xbb\xbf":
        data = data[3:]
    return data.decode("utf-8", "replace")


def windows(xml):
    """{name: (type, parent name or None)} of the named windows, in document order."""
    out = collections.OrderedDict()
    stack = []
    for m in WINDOW.finditer(xml):
        if m.group(1) is None:
            if stack:
                stack.pop()
            continue
        attrs = dict(ATTR.findall(m.group(1)))
        name = attrs.get("Name")
        parent = next((s for s in reversed(stack) if s), None)
        if name:
            out[name] = (attrs.get("Type", ""), parent)
        if m.group(2) != "/":
            stack.append(name)
    return out


def overlap(a, b):
    """(names of a that b has, names of a that b lacks)"""
    have = [n for n in a if n in b]
    return have, [n for n in a if n not in b]


def main():
    if len(sys.argv) not in (2, 3):
        print(__doc__)
        return 2
    a = windows(text_of(open(sys.argv[1], "rb").read()))
    if len(sys.argv) == 2:
        for name, (kind, parent) in a.items():
            print(f"{name}\t{kind}\t{parent or ''}")
        return 0
    b = windows(text_of(open(sys.argv[2], "rb").read()))
    have, missing = overlap(a, b)
    print(f"{len(have)} of {len(a)} named windows of A are in B")
    for n in missing:
        print(f"missing in B: {n}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
