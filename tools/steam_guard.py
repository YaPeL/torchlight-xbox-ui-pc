#!/usr/bin/env python3
"""Snapshot and verify folders that must not change (Steam's Torchlight prefix and cloud folder).

steam_guard.py snapshot OUT.json DIR...   SHA-256 of every file, plus every folder and link, of each DIR
steam_guard.py verify OUT.json            exit 1, listing the differences, if anything changed

Read only: nothing in the DIRs is written. Run `snapshot` before any PC run and `verify` after it.
"""
import hashlib
import json
import os
import sys


def tree(root):
    """{relative path: description} for every file (its SHA-256), folder and link under root."""
    out = {}
    for d, dirs, files in os.walk(root, followlinks=False):
        rel_d = os.path.relpath(d, root)
        out[rel_d + "/"] = "dir"
        for name in sorted(dirs + files):
            p = os.path.join(d, name)
            rel = os.path.relpath(p, root)
            if os.path.islink(p):
                out[rel] = "link -> " + os.readlink(p)
            elif os.path.isfile(p):
                h = hashlib.sha256()
                with open(p, "rb") as f:
                    for block in iter(lambda: f.read(1 << 20), b""):
                        h.update(block)
                out[rel] = h.hexdigest()
    return out


def snapshot(dirs):
    return {os.path.abspath(d): tree(d) for d in dirs}


def differences(before, after):
    lines = []
    for root in sorted(set(before) | set(after)):
        a, b = before.get(root, {}), after.get(root, {})
        for rel in sorted(set(a) | set(b)):
            if rel not in b:
                lines.append(f"gone: {root}/{rel}")
            elif rel not in a:
                lines.append(f"new: {root}/{rel}")
            elif a[rel] != b[rel]:
                lines.append(f"changed: {root}/{rel}")
    return lines


def main():
    if len(sys.argv) >= 4 and sys.argv[1] == "snapshot":
        data = snapshot(sys.argv[3:])
        with open(sys.argv[2], "w") as f:
            json.dump(data, f, indent=0, sort_keys=True)
        print(f"{sum(len(t) for t in data.values())} entries in {len(data)} folder(s)")
        return 0
    if len(sys.argv) == 3 and sys.argv[1] == "verify":
        with open(sys.argv[2]) as f:
            before = json.load(f)
        lines = differences(before, snapshot(before.keys()))
        for line in lines:
            print(line)
        print("UNCHANGED" if not lines else f"{len(lines)} difference(s)")
        return 1 if lines else 0
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main())
