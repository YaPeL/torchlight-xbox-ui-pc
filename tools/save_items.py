#!/usr/bin/env python3
"""Lists the items of a PC Torchlight save (.svt), read only: a partial reading of the format, for
comparing a save before and after a test (docs section 21).

    save_items.py SAVE                 the items: their place in the file, owner, slot, name, suffix
    save_items.py compare A B          the items of A and B compared, their slots left out

An item's record, as read from the owner's test saves (not a full description of the format):
  8 bytes (a GUID; the first two 0xDE 0x11), u16 n, the name (n UTF-16 code units), u16 a, the
  prefix ("Prickly [ITEM]"; a units, 0 for none), u16 b, the suffix ("[ITEM] of the Savage"), 8
  bytes (a GUID; the first two 0xF1 0x11), 33 bytes, the slot (i32: 0 to 18 equipped, 19 on the bag, -1 a unit not in an inventory), then
  the rest (its effects' names among them) up to the next record.
Records are told by that pattern. A record's owner is guessed from the file's order (owners()):
the last unit's name before it; the comparison does not rely on it.

The comparison takes each item's bytes from its record's start to the next record's, with its four
slot bytes and the 8-byte ids that start with 0xF1 0x11 (made anew at each save) zeroed, and
pairs the two files' records owner by owner (owners(): the hero, the pet, the stash, each merchant,
each level's units), equal or one a prefix of the other; then the hero's and the pet's together (an
item may move between them). The exit status is 0 when those and the stash pair off.
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
            at = i + 10 + 2 * n
            prefix = suffix = None
            if at + 2 <= len(data):
                a = struct.unpack_from("<H", data, at)[0]
                prefix = _text(data, at + 2, a) if a <= 80 else None
            if prefix is not None:
                at += 2 + 2 * a
                b = struct.unpack_from("<H", data, at)[0] if at + 2 <= len(data) else 0xFFFF
                suffix = _text(data, at + 2, b) if b <= 80 else None
            after = at + 2 + 2 * b if suffix is not None else 0
            if suffix is not None and data[after:after + 2] == TYPE_MARK and after + 45 <= len(data):
                slot_at = after + 41
                out.append((i, i + 10 + 2 * n, name, " ".join(x for x in (prefix, suffix) if x),
                            slot_at, struct.unpack_from("<i", data, slot_at)[0]))
                i = slot_at
                continue
        i += 1


def unit_names(data, recs):
    """[(offset, name)] of the units' names: mixed case, not an affix, outside the records' heads."""
    import re
    heads = [(r[0], r[4] + 4) for r in recs]
    names, h = [], 0
    for m in re.finditer(rb"(?:[\x20-\x7e]\x00){3,}", data):
        t = m.group().decode("utf-16-le")
        if t.upper() == t or "[ITEM]" in t:
            continue
        while h < len(heads) and heads[h][1] <= m.start():
            h += 1
        if h < len(heads) and heads[h][0] <= m.start() < heads[h][1]:
            continue
        names.append((m.start(), t))
    return names


def owners(data, recs):
    """Each record's owner as read from the file's order: the last name before it outside the
    records' heads that is in mixed case and not an affix (the hero's in the header, then each
    unit's: the pet's, a level's units; effects' and levels' names are in capitals). A socketed
    gem (slot -1 right after its item) has its item's owner."""
    names = unit_names(data, recs)
    out, k, last = [], 0, ""
    for r in recs:
        while k < len(names) and names[k][0] < r[0]:
            last = names[k][1]
            k += 1
        out.append(last)
    return out


def items(path):
    with open(path, "rb") as f:
        data = f.read()
    return data, records(data)


def masked(data, recs):
    """The records' bytes, the slot zeroed, each up to the next record or the next unit's name
    (the last item of a unit is not followed by the next unit's header)."""
    import bisect
    starts = [o for o, _ in unit_names(data, recs)]
    out = []
    for k, r in enumerate(recs):
        end = recs[k + 1][0] if k + 1 < len(recs) else len(data)
        j = bisect.bisect_right(starts, r[4])
        if j < len(starts) and starts[j] < end:
            end = starts[j] - 2                  # its length's two bytes before it
        end = min(end, recs[k + 1][0] if k + 1 < len(recs) else len(data))
        b = bytearray(data[r[0]:end])
        b[r[4] - r[0]:r[4] - r[0] + 4] = b"\0\0\0\0"
        k2 = b.find(TYPE_MARK)
        while k2 >= 0:                       # the save's instance ids (0xF1 0x11 and 6 bytes), new at each save
            b[k2:k2 + 8] = b"\0" * len(b[k2:k2 + 8])
            k2 = b.find(TYPE_MARK, k2 + 8)
        out.append(bytes(b))
    return out


def unmatched(pa, pb):
    """The (label, bytes) of A and of B left when each A record is paired with a B record of the
    same label whose bytes are equal, or one a prefix of the other (a unit's last item runs on into
    the next unit's header, which differs between saves)."""
    left_b = list(pb)
    left_a = []
    for n, m in pa:
        for k, (nb, mb) in enumerate(left_b):
            if nb == n and (mb.startswith(m) or m.startswith(mb)):
                del left_b[k]
                break
        else:
            left_a.append((n, m))
    return left_a, left_b


def main(argv):
    if len(argv) == 1:
        data, recs = items(argv[0])
        for r, o in zip(recs, owners(data, recs)):
            print(f"{r[0]:#08x} {r[5]:>5} {o[:22]:<22} {r[2]}" + (f" / {r[3]}" if r[3] else ""))
        print(f"{len(recs)} records")
        return 0
    if len(argv) == 3 and argv[0] == "compare":
        (da, ra), (db, rb) = items(argv[1]), items(argv[2])
        label = lambda r: r[2] + (f" / {r[3]}" if r[3] else "")
        groups = {}
        for side, (data, recs) in enumerate(((da, ra), (db, rb))):
            for r, o, m in zip(recs, owners(data, recs), masked(data, recs)):
                groups.setdefault(o, ([], []))[side].append((label(r), m, r[5]))
        order = list(dict.fromkeys(owners(da, ra)))
        watched = set(order[:2]) | {"Stash"}       # the hero (the header's name), the pet (the next unit), the stash
        for o, (pa, pb) in groups.items():
            na, nb = collections.Counter(n for n, _, _ in pa), collections.Counter(n for n, _, _ in pb)
            ua, ub = unmatched([(n, m) for n, m, _ in pa], [(n, m) for n, m, _ in pb])
            missing, added = na - nb, nb - na
            changed = len(ua) - sum(missing.values())
            same = not ua and not ub
            print(f"{o or '(before any name)'}: A {len(pa)}, B {len(pb)}: " + ("the same" if same else "differ")
                  + (" (watched)" if o in watched else ""))
            for n, k in sorted(missing.items()):
                print(f"  only in A ({k}): {n}")
            for n, k in sorted(added.items()):
                print(f"  only in B ({k}): {n}")
            if changed:
                print(f"  {changed} with the same name but other bytes:", ", ".join(sorted({n for n, _ in ua} - set(missing))))
        both_a = [(n, m) for o in order[:2] for n, m, _ in groups[o][0]]
        both_b = [(n, m) for o in order[:2] for n, m, _ in groups.get(o, ([], []))[1]]
        ua, ub = unmatched(both_a, both_b)
        print(f"the hero and the pet together ({' + '.join(order[:2])}): A {len(both_a)}, B {len(both_b)}: "
              + ("the same" if not ua and not ub else f"differ ({len(ua)} of A, {len(ub)} of B unmatched)"))
        stash = groups.get("Stash")
        stash_same = not stash or not any(unmatched([(n, m) for n, m, _ in stash[0]], [(n, m) for n, m, _ in stash[1]]))
        return 0 if not ua and not ub and stash_same else 1
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
