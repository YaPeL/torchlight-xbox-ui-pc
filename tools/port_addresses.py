#!/usr/bin/env python3
"""Find the GOG addresses the proxy uses in another build of Torchlight.exe (Steam's), offline.

port_addresses.py GOG_EXE OTHER_EXE [ADDRESS ...] [--source DIR]

Both builds come from the same code (docs section 5), so a run of GOG's bytes at an address
usually appears once in the other build, once the bytes that hold addresses are masked:
call and jump displacements (E8, E9, 0F 8x) and any four bytes whose value falls inside the
image. A code address is matched by a window of bytes from it, grown until one match is left;
a data address (outside the code section) is found through the code that names it: each place in
GOG's code holding the address is matched in the other build and the four bytes there are read.
With --source, the addresses are the constants 0x400000..0x6fffff written in DIR's .c files.

Nothing is run: both files are only read. Prints one line per address:
GOG address, the other build's address (or "-"), and how it was found.
"""
import argparse
import os
import re
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from pe_info import PE  # noqa: E402

WINDOWS = (16, 24, 32, 48, 64, 96, 128)
LOW, HIGH = 0x400000, 0x700000


class Image:
    def __init__(self, data):
        self.pe = PE(data)
        self.data = data
        self.base = self.pe.image_base
        self.end = self.base + max(va + max(vs, rs) for _, va, vs, _, rs in self.pe.sections)
        code = [s for s in self.pe.sections if s[0] == ".text"] or self.pe.sections[:1]
        _, self.code_va, vsize, self.code_raw, rsize = code[0]
        self.code = data[self.code_raw:self.code_raw + min(vsize, rsize)]

    def in_code(self, va):
        return 0 <= va - self.base - self.code_va < len(self.code)

    def code_offset(self, va):
        return va - self.base - self.code_va

    def code_va_of(self, off):
        return self.base + self.code_va + off

    def read32(self, va):
        o = self.pe.offset(va - self.base)
        return struct.unpack_from("<I", self.data, o)[0] if o is not None else None


def mask(image, chunk):
    """The bytes of chunk with displacements and in-image values marked as unknown (None)."""
    out = list(chunk)
    n = len(chunk)
    for i in range(n):
        if chunk[i] in (0xE8, 0xE9) and i + 5 <= n:
            out[i + 1:i + 5] = [None] * 4
        if chunk[i] == 0x0F and i + 6 <= n and 0x80 <= chunk[i + 1] <= 0x8F:
            out[i + 2:i + 6] = [None] * 4
    for i in range(n - 3):
        v = struct.unpack_from("<I", chunk, i)[0]
        if image.base <= v < image.end:
            out[i:i + 4] = [None] * 4
    return out


def pattern(masked):
    return re.compile(b"".join(b"." if b is None else re.escape(bytes([b])) for b in masked), re.DOTALL)


def find_code(gog, other, va, before=0):
    """The other build's address for GOG's code address va, and the window that matched it."""
    start = gog.code_offset(va) - before
    for size in WINDOWS:
        chunk = gog.code[start:start + before + size]
        if len(chunk) < before + size:
            break
        m = mask(gog, chunk)
        if sum(b is not None for b in m) < 8:
            continue
        hits = [h.start() for h in pattern(m).finditer(other.code)]
        if len(hits) == 1:
            return other.code_va_of(hits[0] + before), size
        if not hits:
            return None, size
    return None, None


def find_data(gog, other, va, limit=8):
    """The other build's address for GOG's data address va, through the code that holds it."""
    needle = struct.pack("<I", va)
    found = {}
    pos = gog.code.find(needle)
    tried = 0
    while pos >= 0 and tried < limit:
        tried += 1
        site = gog.code_va_of(pos)
        for back in (2, 8, 16, 32, 0):
            if pos - back < 0:
                continue
            there, _ = find_code(gog, other, site, before=back)
            if there is not None:
                value = other.read32(there)
                found[value] = found.get(value, 0) + 1
                break
        pos = gog.code.find(needle, pos + 1)
    if len(found) == 1:
        (value, count), = found.items()
        return value, f"data, {count} of {tried} references agree"
    if found:
        return None, "data, references disagree: " + ", ".join(f"{v:#x}" for v in found)
    return None, f"data, none of {tried} references matched"


def port(gog, other, va):
    if not gog.in_code(va):
        return find_data(gog, other, va)
    there, size = find_code(gog, other, va)
    if there is not None:
        return there, f"code, unique in {size} bytes"
    # A window ending at the address: the address may be near a function's end.
    for back in (16, 32, 64):
        there, size = find_code(gog, other, va, before=back)
        if there is not None:
            return there, f"code, unique in {back} bytes before and {size} after"
    return None, "code, no unique match"


def source_addresses(folder):
    found = set()
    for name in sorted(os.listdir(folder)):
        if name.endswith(".c"):
            text = open(os.path.join(folder, name), encoding="utf-8").read()
            for m in re.finditer(r"\b0x([0-9a-fA-F]{6})\b", text):
                v = int(m.group(1), 16)
                if LOW <= v < HIGH:
                    found.add(v)
    return sorted(found)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("gog")
    ap.add_argument("other")
    ap.add_argument("addresses", nargs="*")
    ap.add_argument("--source")
    a = ap.parse_args(argv)
    gog = Image(open(a.gog, "rb").read())
    other = Image(open(a.other, "rb").read())
    addresses = [int(x, 16) for x in a.addresses] + (source_addresses(a.source) if a.source else [])
    missing = 0
    for va in addresses:
        there, how = port(gog, other, va)
        missing += there is None
        print(f"{va:#08x} {there:#08x} {how}" if there is not None else f"{va:#08x} - {how}")
    print(f"{len(addresses) - missing} of {len(addresses)} found", file=sys.stderr)
    return 1 if missing else 0


if __name__ == "__main__":
    sys.exit(main())
