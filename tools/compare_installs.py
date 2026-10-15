#!/usr/bin/env python3
"""Compare the executables and DLLs of two PC installs (e.g. GOG against Steam), reading files only.

compare_installs.py DIR_A DIR_B
For every .exe and .dll in either folder (matched by path, ignoring case): missing on one side,
identical (SHA-256), or different, and then how: size, link timestamp, sections, imports and exports.
"""
import hashlib
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from pe_info import PE  # noqa: E402

KINDS = (".exe", ".dll")


def binaries(root):
    out = {}
    for d, _, files in os.walk(root):
        for f in files:
            if f.lower().endswith(KINDS):
                p = os.path.join(d, f)
                out[os.path.relpath(p, root).replace("\\", "/").lower()] = p
    return out


def describe(a, b):
    """How two different PE files differ, as lines."""
    da, db = open(a, "rb").read(), open(b, "rb").read()
    lines = [f"size {len(da)} vs {len(db)}"]
    try:
        pa, pb = PE(da), PE(db)
    except ValueError as e:
        return lines + [f"not comparable as PE: {e}"]
    if pa.timestamp != pb.timestamp:
        lines.append(f"link timestamp {pa.timestamp} vs {pb.timestamp}")
    sa = [(n, v, s) for n, v, s, _, _ in pa.sections]
    sb = [(n, v, s) for n, v, s, _, _ in pb.sections]
    lines.append("sections the same (name, address, size)" if sa == sb else f"sections {sa} vs {sb}")
    ia, ib = pa.imports(), pb.imports()
    for dll in sorted(set(ia) | set(ib), key=str.lower):
        fa, fb = set(ia.get(dll, [])), set(ib.get(dll, []))
        if fa != fb:
            lines.append(f"imports from {dll}: only A {sorted(fa - fb)}, only B {sorted(fb - fa)}")
    ea, eb = {n for n, _ in pa.exports()}, {n for n, _ in pb.exports()}
    if ea != eb:
        lines.append(f"exports: only A {len(ea - eb)}, only B {len(eb - ea)}")
    return lines


def compare(root_a, root_b):
    a, b = binaries(root_a), binaries(root_b)
    report = []
    for key in sorted(set(a) | set(b)):
        if key not in b:
            report.append((key, "only in A", []))
        elif key not in a:
            report.append((key, "only in B", []))
        else:
            ha = hashlib.sha256(open(a[key], "rb").read()).hexdigest()
            hb = hashlib.sha256(open(b[key], "rb").read()).hexdigest()
            report.append((key, "identical", []) if ha == hb else (key, "different", describe(a[key], b[key])))
    return report


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    for key, verdict, lines in compare(sys.argv[1], sys.argv[2]):
        print(f"{verdict:10} {key}")
        for line in lines:
            print(f"           {line}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
