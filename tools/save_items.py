#!/usr/bin/env python3
"""Lists the items of a PC Torchlight save (.svt), read only: a partial reading of the format, for
comparing a save before and after a test (docs section 21).

    save_items.py SAVE                 the items: their place in the file, owner, slot, name, suffix
    save_items.py compare A B          the items of A and B compared, their slots left out

An item's record, as read from the owner's test saves (not a full description of the format):
  8 bytes (a GUID; the first two 0xDE 0x11), u16 n, the name (n UTF-16 code units), a u16 (0 in
  the saves read), u16 m, the suffix ("[ITEM] of the Savage"; m units, 0 for none), 8 bytes (a
  GUID; the first two 0xF1 0x11), 33 bytes, the slot (i32: 0 to 18 equipped, 19 on the bag, -1 a unit not in an inventory), then
  the rest (its effects' names among them) up to the next record.
Records are told by that pattern. Who owns a record (the hero, the pet, a stash, a merchant, a
level) is not read: the file's order is not described here.

The comparison takes each item's bytes from its record's start to the next record's, with its four
slot bytes and the 8-byte ids that start with 0xF1 0x11 (made anew at each save) zeroed, and
compares the two files' lists as multisets, apart for the records in an inventory (slot 0 on) and
the others (slot -1: a level's units); the exit status is 1 when the inventories' differ.
"""
import collections
import struct
import sys

ITEM_MARK, TYPE_MARK = b"\xde\x11", b"\xf1\x11"


def _text(data, at, units):
    raw = data[at:at + 2 * units]
    if len(raw) != 2 * units:
        return None
    try:
        t = raw.decode("utf-16-le")
    except UnicodeDecodeError:
        return None
    return t if t.isprintable() else None


def records(data):
    """[(offset, end of the name, name, suffix, slot offset, slot)] of the item-like records."""
    out, i = [], 0
    while True:
        i = data.find(ITEM_MARK, i)
        if i < 0 or i + 12 > len(data):
            return out
        n = struct.unpack_from("<H", data, i + 8)[0]
        name = _text(data, i + 10, n) if 1 <= n <= 80 else None
        if name:
            at = i + 10 + 2 * n + 2
            m = struct.unpack_from("<H", data, at)[0] if at + 2 <= len(data) else 0xFFFF
            suffix = _text(data, at + 2, m) if m <= 80 else None
            after = at + 2 + 2 * m
            if suffix is not None and data[after:after + 2] == TYPE_MARK and after + 45 <= len(data):
                slot_at = after + 41
                out.append((i, i + 10 + 2 * n, name, suffix, slot_at, struct.unpack_from("<i", data, slot_at)[0]))
                i = slot_at
                continue
        i += 1


def items(path):
    with open(path, "rb") as f:
        data = f.read()
    return data, records(data)


def masked(data, recs):
    """The records' bytes, the slot zeroed, each up to the next record."""
    out = []
    for k, r in enumerate(recs):
        end = recs[k + 1][0] if k + 1 < len(recs) else len(data)
        b = bytearray(data[r[0]:end])
        b[r[4] - r[0]:r[4] - r[0] + 4] = b"\0\0\0\0"
        k2 = b.find(TYPE_MARK)
        while k2 >= 0:                       # the save's instance ids (0xF1 0x11 and 6 bytes), new at each save
            b[k2:k2 + 8] = b"\0" * len(b[k2:k2 + 8])
            k2 = b.find(TYPE_MARK, k2 + 8)
        out.append(bytes(b))
    return out


def main(argv):
    if len(argv) == 1:
        data, recs = items(argv[0])
        for r in recs:
            print(f"{r[0]:#08x} {r[5]:>5} {r[2]}" + (f" / {r[3]}" if r[3] else ""))
        print(f"{len(recs)} records")
        return 0
    if len(argv) == 3 and argv[0] == "compare":
        (da, ra), (db, rb) = items(argv[1]), items(argv[2])
        label = lambda r: r[2] + (f" / {r[3]}" if r[3] else "")
        result = 0
        for title, keep in (("in an inventory (slot 0 on)", lambda r: r[5] >= 0), ("not in one (slot -1)", lambda r: r[5] < 0)):
            pa = [(label(r), m) for r, m in zip(ra, masked(da, ra)) if keep(r)]
            pb = [(label(r), m) for r, m in zip(rb, masked(db, rb)) if keep(r)]
            na, nb = collections.Counter(n for n, _ in pa), collections.Counter(n for n, _ in pb)
            ca, cb = collections.Counter(pa), collections.Counter(pb)
            missing, added = na - nb, nb - na
            changed = sum((ca - cb).values()) - sum(missing.values())
            print(f"{title}: A {len(pa)}, B {len(pb)}")
            for n, k in sorted(missing.items()):
                print(f"  only in A ({k}): {n}")
            for n, k in sorted(added.items()):
                print(f"  only in B ({k}): {n}")
            if changed:
                print(f"  {changed} with the same name in both but other bytes:",
                      ", ".join(sorted({n for n, _ in (ca - cb)} - set(missing))))
            if keep(( 0, 0, "", "", 0, 0)) and (missing or added or changed):
                result = 1
        return result
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
