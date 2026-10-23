#!/usr/bin/env python3
"""Compare the struct offsets two builds' copies of the same functions use, offline.

compare_offsets.py GOG_EXE OTHER_EXE TABLE [--all]

TABLE is port_addresses.py's (or proxy/addresses_steam.txt): "GOG OTHER status ..." lines. For each
pair that is a function start in both builds, both functions are disassembled (GNU objdump, Intel
syntax; nothing is run) and the displacements of their memory operands on a register other than
esp and ebp (the fields of an object: [esi+0x304], not the stack, whose frame differs between the
builds) are listed in order. The two lists are aligned (difflib); a function whose lists match is
"same", else the differing offsets are printed. With --all, the matching ones are printed too.
"""
import argparse
import difflib
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from port_addresses import Image  # noqa: E402

OBJDUMP = "objdump"
LINE = re.compile(r"^\s*([0-9a-f]+):\s+((?:[0-9a-f]{2} )+)\s*(.*)$")
MEMORY = re.compile(r"\[([a-z]{3})(?:\+[a-z]{3}\*\d)?([+-]0x[0-9a-f]+)\]")


def instructions(text):
    """[(address, mnemonic and operands)] of objdump -d output."""
    out = []
    for line in text.splitlines():
        m = LINE.match(line)
        if m and m.group(3):
            out.append((int(m.group(1), 16), m.group(3).strip()))
    return out


def function_body(instrs):
    """The instructions up to the function's end: the first int3 (the padding between functions;
    the compiler puts none inside one)."""
    for i, (_, text) in enumerate(instrs):
        if text.split()[0] == "int3":
            return instrs[:i]
    return instrs


def field_offsets(instrs):
    """The object field displacements, in order: [(mnemonic, displacement)]. Left out: the stack's
    (esp, ebp), the alignment filler (lea r,[r+0x0]) and values of 64 KB or more (data decoded as
    code, such as a jump table after the function)."""
    out = []
    for _, text in instrs:
        mnemonic = text.split()[0]
        for reg, disp in MEMORY.findall(text):
            d = int(disp, 16)
            if reg in ("esp", "ebp") or (mnemonic == "lea" and d == 0) or abs(d) >= 0x10000:
                continue
            out.append((mnemonic, d))
    return out


def disassemble(path, start, end):
    r = subprocess.run([OBJDUMP, "-d", "-M", "intel", "-w",
                        f"--start-address={start:#x}", f"--stop-address={end:#x}", path],
                       capture_output=True, text=True, check=True)
    return instructions(r.stdout)


def compare(a, b):
    """The differences between two field offset lists: [(kind, a's, b's)]."""
    sm = difflib.SequenceMatcher(a=a, b=b, autojunk=False)
    return [(tag, a[i1:i2], b[j1:j2]) for tag, i1, i2, j1, j2 in sm.get_opcodes() if tag != "equal"]


def read_table(path):
    pairs = []
    with open(path) as f:
        for line in f:
            parts = line.split()
            if len(parts) >= 3 and not line.startswith("#") and parts[1] != "-":
                pairs.append((int(parts[0], 16), int(parts[1], 16), parts[2]))
    return pairs


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("gog")
    ap.add_argument("other")
    ap.add_argument("table")
    ap.add_argument("--all", action="store_true")
    a = ap.parse_args(argv)
    with open(a.gog, "rb") as f:
        gog = Image(f.read())
    with open(a.other, "rb") as f:
        other = Image(f.read())
    same = differ = 0
    for g, o, status in read_table(a.table):
        if not (gog.in_code(g) and g in set(gog.starts()) and other.in_code(o)):
            continue
        gi = function_body(disassemble(a.gog, g, g + 0x4000))
        oi = function_body(disassemble(a.other, o, o + 0x4000))
        ga, oa = field_offsets(gi), field_offsets(oi)
        diffs = compare(ga, oa)
        if not diffs:
            same += 1
            if a.all:
                print(f"{g:#010x} {o:#010x} same ({len(ga)} field operands)")
            continue
        differ += 1
        shown = "; ".join(f"{tag} {[f'{m} {d:#x}' for m, d in x]} -> {[f'{m} {d:#x}' for m, d in y]}"
                          for tag, x, y in diffs[:6])
        print(f"{g:#010x} {o:#010x} DIFFER ({len(ga)} / {len(oa)} field operands): {shown}")
    print(f"{same} same, {differ} differ", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
