#!/usr/bin/env python3
"""Find the GOG addresses the proxy uses in another build of Torchlight.exe (Steam's), offline.

port_addresses.py GOG_EXE OTHER_EXE [ADDRESS ...] [--source DIR] [--code-only] [--table FILE]

Both builds come from the same code (docs section 5), so most of GOG's code appears in the other
build once the bytes that hold addresses are masked: call and jump displacements (E8, E9, 0F 8x)
and any four bytes whose value falls inside the image. Each address is looked for in several
independent ways, and the ways that agree are written down:

- window:  a window of GOG's bytes from the address, grown until one match is left in the other
           build's code;
- body:    windows from inside the function (the prologue may differ: GOG's build has stack
           cookies, the other's not), the match's function start taken (the function starts are
           the 16-aligned call targets and vtable entries);
- callers: GOG's calls to the function, each call site matched, the other build's call target read;
- local:   for an address inside a function (a patch site), the window matched only inside the
           other build's copy of the function, itself found as above;
- refs:    for a data address, GOG's code that holds it, each site matched (inside its function's
           copy, else in the whole code) and the four bytes there read;
- rtti:    for a vtable, its class's RTTI name (and the locator's offsets) in the other build;
- import:  for an import slot, the same DLL's function in the other build's import table;
- content: for constant data (floats, strings), the bytes there compared, as a check.

An address is "verified" when two independent ways agree and none disagrees, "single" when one way
alone finds it, "unresolved" when none does or they disagree. Nothing is run: both files are only
read. With --source, the addresses are the constants 0x400000..0xffffff written in DIR's .c files
(with --code-only, outside comments). --table writes "GOG OTHER status evidence" lines.
"""
import argparse
import bisect
import os
import re
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from pe_info import PE  # noqa: E402

WINDOWS = (16, 24, 32, 48, 64, 96, 128)
LOW, HIGH = 0x400000, 0x1000000


class Image:
    def __init__(self, data):
        self.pe = PE(data)
        self.data = data
        self.base = self.pe.image_base
        self.end = self.base + max(va + max(vs, rs) for _, va, vs, _, rs in self.pe.sections)
        code = [s for s in self.pe.sections if s[0] == ".text"] or self.pe.sections[:1]
        _, self.code_va, vsize, self.code_raw, rsize = code[0]
        self.code = data[self.code_raw:self.code_raw + min(vsize, rsize)]
        self._starts = None
        self._calls = None
        self._iat = None

    # ---- addresses ----
    def in_code(self, va):
        return 0 <= va - self.base - self.code_va < len(self.code)

    def code_offset(self, va):
        return va - self.base - self.code_va

    def code_va_of(self, off):
        return self.base + self.code_va + off

    def section_of(self, va):
        for name, sva, vs, _, rs in self.pe.sections:
            if self.base + sva <= va < self.base + sva + max(vs, rs):
                return name
        return None

    def read(self, va, n):
        o = self.pe.offset(va - self.base)
        return self.data[o:o + n] if o is not None else b""

    def read32(self, va):
        b = self.read(va, 4)
        return struct.unpack("<I", b)[0] if len(b) == 4 else None

    def section_data(self, name):
        for n, sva, vs, raw, rs in self.pe.sections:
            if n == name:
                return self.base + sva, self.data[raw:raw + min(vs, rs) if vs else rs]
        return None, b""

    # ---- the call graph ----
    def calls(self):
        """{target: [call sites]} for every E8 rel32 whose target is a 16-aligned code address."""
        if self._calls is None:
            out = {}
            code, n = self.code, len(self.code)
            pos = code.find(b"\xe8")
            while 0 <= pos <= n - 5:
                t = pos + 5 + struct.unpack_from("<i", code, pos + 1)[0]
                if 0 <= t < n and (self.code_va_of(t) & 15) == 0:
                    out.setdefault(self.code_va_of(t), []).append(self.code_va_of(pos))
                pos = code.find(b"\xe8", pos + 1)
            self._calls = out
        return self._calls

    def calls_to(self, va):
        return self.calls().get(va, [])

    def starts(self):
        """The function starts: 16-aligned call targets and 16-aligned code pointers in .rdata."""
        if self._starts is None:
            s = set(self.calls())
            rva, rdata = self.section_data(".rdata")
            for i in range(0, len(rdata) - 3, 4):
                v = struct.unpack_from("<I", rdata, i)[0]
                if (v & 15) == 0 and self.in_code(v):
                    s.add(v)
            # And the 16-aligned code after int3 padding (functions only called through pointers).
            code = self.code
            for off in range(16, len(code), 16):
                if code[off - 1] == 0xCC and code[off] != 0xCC:
                    s.add(self.code_va_of(off))
            self._starts = sorted(s)
        return self._starts

    def function_of(self, va):
        s = self.starts()
        i = bisect.bisect_right(s, va) - 1
        return s[i] if i >= 0 else None

    def next_start(self, va):
        s = self.starts()
        i = bisect.bisect_right(s, va)
        return s[i] if i < len(s) else self.code_va_of(len(self.code))

    # ---- imports ----
    def iat(self):
        """{slot VA: (dll lower case, function name)} of the import table."""
        if self._iat is None:
            out = {}
            pe, d = self.pe, self.data
            if len(pe.dirs) > 1 and pe.dirs[1][0]:
                o = pe.offset(pe.dirs[1][0])
                while True:
                    lookup, _, _, name, first = struct.unpack_from("<IIIII", d, o)
                    if not (lookup or name or first):
                        break
                    dll = pe.cstr(name).lower()
                    t = pe.offset(lookup or first)
                    slot = first
                    while True:
                        v = struct.unpack_from("<I", d, t)[0]
                        if not v:
                            break
                        out[self.base + slot] = (dll, f"#{v & 0xFFFF}" if v & 0x80000000 else pe.cstr(v + 2))
                        t += 4
                        slot += 4
                    o += 20
            self._iat = out
        return self._iat

    # ---- RTTI ----
    def rtti(self, vtable):
        """(class name, offset, cd offset) of the vtable's complete object locator, or None."""
        col = self.read32(vtable - 4)
        if col is None or self.section_of(col) != ".rdata":
            return None
        b = self.read(col, 20)
        if len(b) < 20:
            return None
        sig, off, cd, td, _ = struct.unpack("<IIIII", b)
        if sig != 0 or self.section_of(td) not in (".data", ".rdata"):
            return None
        name = self.read(td + 8, 256)
        if not name.startswith(b".?A") or b"\0" not in name:
            return None
        return name[:name.index(b"\0")].decode("latin1"), off, cd

    def vtables_of(self, name, off, cd):
        """The vtables whose locator names this class with these offsets."""
        needle = b".?A" + name[3:].encode("latin1") + b"\0"
        out = []
        for sec in (".data", ".rdata"):
            sva, body = self.section_data(sec)
            p = body.find(needle)
            while p >= 0:
                td = sva + p - 8
                rva, rdata = self.section_data(".rdata")
                q = rdata.find(struct.pack("<I", td))
                while q >= 0:
                    col = rva + q - 12
                    if (q - 12) % 4 == 0 and self.read(col, 12) == struct.pack("<III", 0, off, cd):
                        r = rdata.find(struct.pack("<I", col))
                        while r >= 0:
                            if r % 4 == 0:
                                out.append(rva + r + 4)
                            r = rdata.find(struct.pack("<I", col), r + 1)
                    q = rdata.find(struct.pack("<I", td), q + 1)
                p = body.find(needle, p + 1)
        return sorted(set(out))


def mask(image, chunk, loose=False):
    """The bytes of chunk with displacements and in-image values marked as unknown (None). With
    loose, also the stack displacements ([esp+d] through a SIB byte, [ebp+d]), which differ
    between builds whose frames differ (GOG's stack cookie); it is only used inside a small region."""
    out = list(chunk)
    n = len(chunk)
    if loose:
        for i in range(n - 1):
            b = chunk[i]
            if (b & 0xC7) == 0x44 and chunk[i + 1] == 0x24 and i + 2 < n:
                out[i + 2] = None
            elif (b & 0xC7) == 0x84 and chunk[i + 1] == 0x24:
                out[i + 2:min(n, i + 6)] = [None] * (min(n, i + 6) - i - 2)
            elif (b & 0xC7) == 0x45:
                out[i + 1] = None
            elif (b & 0xC7) == 0x85:
                out[i + 1:min(n, i + 5)] = [None] * (min(n, i + 5) - i - 1)
    for i in range(n):
        # A displacement cut by the window's end is masked as far as it goes.
        if chunk[i] in (0xE8, 0xE9):
            out[i + 1:min(n, i + 5)] = [None] * (min(n, i + 5) - i - 1)
        if chunk[i] == 0x0F and i + 1 < n and 0x80 <= chunk[i + 1] <= 0x8F:
            out[i + 2:min(n, i + 6)] = [None] * max(0, min(n, i + 6) - i - 2)
    for i in range(n - 3):
        v = struct.unpack_from("<I", chunk, i)[0]
        if image.base <= v < image.end:
            out[i:i + 4] = [None] * 4
    return out


def pattern(masked):
    return re.compile(b"".join(b"." if b is None else re.escape(bytes([b])) for b in masked), re.DOTALL)


def same_constants(gog, other, gog_off, other_off, n):
    """True if every value in GOG's code[gog_off:+n] that points into .rdata points at the same
    16 bytes (in-image values masked) as the value at the same place of the other's code."""
    for i in range(n - 3):
        v = struct.unpack_from("<I", gog.code, gog_off + i)[0]
        if gog.base <= v < gog.end and gog.section_of(v) == ".rdata":
            if other_off + i + 4 > len(other.code):
                return False
            w = struct.unpack_from("<I", other.code, other_off + i)[0]
            a, b = gog.read(v, 16), other.read(w, 16)
            if len(a) < 16 or len(b) < 16 or not pattern(mask(gog, a)).fullmatch(b):
                return False
    return True


def find_code(gog, other, va, before=0, region=None, loose=False):
    """The other build's address for GOG's code address va, and the window that matched it.
    With region (start, end), the match is looked for, and must be unique, only there; loose
    masks the stack displacements too (mask())."""
    start = gog.code_offset(va) - before
    if start < 0:
        return None, None
    lo, hi = 0, len(other.code)
    if region:
        lo, hi = max(0, other.code_offset(region[0])), min(len(other.code), other.code_offset(region[1]))
    misses = 0
    for size in WINDOWS:
        chunk = gog.code[start:start + before + size]
        if len(chunk) < before + size:
            break
        m = mask(gog, chunk, loose)
        if sum(b is not None for b in m) < (12 if loose else 8):
            continue
        hits = [h.start() for h in pattern(m).finditer(other.code, lo, min(len(other.code), hi + before + size))]
        hits = [h for h in hits if lo <= h + before < hi]
        if 1 < len(hits) <= 64:
            # Code that differs only by the constants it names (a getter of one property name
            # among many): the hits whose .rdata pointers point at what GOG's point at.
            hits = [h for h in hits if same_constants(gog, other, start, h, len(chunk))] or hits
        if len(hits) == 1:
            return other.code_va_of(hits[0] + before), size
        if not hits:
            # A longer window cannot match where this one did not, unless a value was cut by
            # this one's end: one size more, then stop.
            misses += 1
            if misses == 2:
                break
    return None, None


class Evidence:
    """Candidates and the ways that found each."""

    def __init__(self):
        self.votes = {}

    def add(self, value, how):
        if value is not None:
            self.votes.setdefault(value, []).append(how)

    def verdict(self):
        """(value, status, text)."""
        if not self.votes:
            return None, "unresolved", "no way found it"
        ranked = sorted(self.votes.items(), key=lambda kv: -len(kv[1]))
        text = "; ".join(f"{v:#x}: " + ", ".join(hs) for v, hs in ranked)
        # A call site or reference matched in the whole code, not inside its function's copy, is
        # weak: it is counted only when nothing else found the address, and does not overrule
        # what did.
        strong = [(v, [h for h in hs if "-global" not in h.split(" ")[0]]) for v, hs in ranked]
        strong = [(v, hs) for v, hs in strong if hs]
        if strong:
            ranked = strong
        if len(ranked) > 1:
            # One call site or reference matched to another place (a small caller like many
            # others) is outvoted by three independent ways; anything more is a disagreement.
            top = independent(ranked[0][1])
            rest = [hs for _, hs in ranked[1:]]
            if top >= 3 and all(len(hs) == 1 and hs[0].split(" ")[0] in ("callers", "refs") for hs in rest):
                return ranked[0][0], "verified", text + " (the others outvoted)"
            return None, "unresolved", "ways disagree: " + text
        value, hows = ranked[0]
        return value, "verified" if independent(hows) >= 2 else "single", text


def independent(hows):
    """How many independent ways: a window, the body and a local match (the window inside the
    function's copy found by the other ways) count once each, however many windows each took;
    every call site, reference, RTTI name, import entry, string and anchor is a way of its own."""
    kinds = [h.split(" ")[0] for h in hows]
    return (len({k for k in kinds if k in ("window", "body", "local")})
            + sum(1 for k in kinds if k not in ("window", "body", "local")))


def string_at(image, va, least=6):
    """(the string's bytes with its terminator, wide) if a string of least characters or more
    starts at va, else (None, False)."""
    raw = image.read(va, 512)
    if len(raw) > 2 * least and all(raw[i + 1] == 0 and 32 <= raw[i] < 127 for i in range(0, 2 * least, 2)):
        end = next((i for i in range(0, len(raw) - 1, 2) if raw[i:i + 2] == b"\0\0"), None)
        return (raw[:end + 2], True) if end else (None, False)
    end = raw.find(b"\0")
    if end >= least and all(32 <= c < 127 or c in (9, 10, 13) for c in raw[:end]):
        return raw[:end + 1], False
    return None, False


def find_string(image, needle, wide=False):
    """The one place in image's .rdata where needle starts after a zero character (a whole
    string, not another's tail), or None."""
    rva, rdata = image.section_data(".rdata")
    hits = []
    p = rdata.find(needle)
    while p >= 0 and len(hits) < 2:
        # A string starts after another's terminator, or 4-aligned after other data.
        if p == 0 or p % 4 == 0 or (rdata[p - 2:p] == b"\0\0" and p % 2 == 0 if wide else rdata[p - 1] == 0):
            hits.append(rva + p)
        p = rdata.find(needle, p + 1)
    return hits[0] if len(hits) == 1 else None


def code_refs(image, va):
    """The code addresses holding the 4 bytes of va."""
    needle, out = struct.pack("<I", va), []
    p = image.code.find(needle)
    while p >= 0:
        out.append(image.code_va_of(p))
        p = image.code.find(needle, p + 1)
    return out


class Porter:
    def __init__(self, gog, other):
        self.gog, self.other = gog, other
        self.functions = {}

    # ---- code ----
    def by_window(self, va, ev):
        there, size = find_code(self.gog, self.other, va)
        if there is not None:
            ev.add(there, f"window {size}")
            return there
        for back in (16, 32, 64):
            there, size = find_code(self.gog, self.other, va, before=back)
            if there is not None:
                ev.add(there, f"window {back}+{size}")
                return there
        return None

    def by_body(self, f, ev):
        """Windows from inside GOG's function f, each match's function start in the other build;
        counted when two windows agree and none disagrees."""
        length = min(self.gog.next_start(f) - f, 1024)
        found = []
        k = 8
        while k + 16 <= length and len(found) < 3:
            there, size = find_code(self.gog, self.other, f + k)
            if there is None or k + size > length:
                k += 8
                continue
            start = self.other.function_of(there)
            if start is not None and abs((there - start) - k) <= 64:
                found.append((start, k))
            k += max(size, 8)
        starts = {st for st, _ in found}
        if len(found) >= 2 and len(starts) == 1:
            ev.add(found[0][0], "body " + ",".join(f"+{k:#x}" for _, k in found))

    def site_in(self, site, depth, back_list=(16, 8, 32, 2)):
        """GOG's code address site in the other build, matched inside its function's copy (or, at
        depth > 0 or when that copy is not found, in the whole code): (address, local)."""
        f = self.gog.function_of(site)
        if depth == 0 and f is not None and f != site:
            start = self.function(f, depth=1)[0]
            if start is not None:
                region = self.region(f, start)
                for loose in (False, True):
                    for back in back_list:
                        there, _ = find_code(self.gog, self.other, site, before=back, region=region, loose=loose)
                        if there is not None:
                            return there, True
        for back in back_list:
            there, _ = find_code(self.gog, self.other, site, before=back)
            if there is not None:
                return there, False
        return None, False

    def by_callers(self, f, ev, depth, limit=6):
        for site in self.gog.calls_to(f)[:limit]:
            there, local = self.site_in(site, depth)
            if there is not None and self.other.read(there, 1) == b"\xe8":
                target = there + 5 + struct.unpack("<i", self.other.read(there + 1, 4))[0]
                ev.add(target, f"callers {site:#x}" if local else f"callers-global {site:#x}")

    def by_vtables(self, f, ev, limit=4):
        """GOG's vtables that hold f (found by their RTTI), the same slot of the other's."""
        rva, rdata = self.gog.section_data(".rdata")
        needle = struct.pack("<I", f)
        q = rdata.find(needle)
        seen = 0
        while q >= 0 and seen < limit:
            if q % 4 == 0:
                for back in range(0, 0x800, 4):
                    v = rva + q - back
                    rtti = self.gog.rtti(v)
                    if rtti:
                        found = self.other.vtables_of(*rtti)
                        if len(found) == 1:
                            ev.add(self.other.read32(found[0] + back), f"vtable {rtti[0]}+{back:#x}")
                            seen += 1
                        break
                    if back and not self.gog.in_code(self.gog.read32(v) or 0):
                        break   # not a vtable's entry any more
            q = rdata.find(needle, q + 1)

    def by_strings(self, f, ev):
        """The strings f names (in .rdata, 3 characters or more, one place in each build): the
        functions naming each, in address order, are the same in both builds when they are as many;
        f's rank among them gives its copy."""
        end = min(self.gog.next_start(f), f + 512)
        seen = set()
        for va in range(f, end - 3):
            v = self.gog.read32(va)
            if v in seen or not (self.gog.base <= v < self.gog.end) or self.gog.section_of(v) != ".rdata":
                continue
            seen.add(v)
            needle, wide = string_at(self.gog, v, least=3)
            there = find_string(self.other, needle, wide) if needle else None
            if there is None or find_string(self.gog, needle, wide) != v:
                continue
            mine = sorted({self.gog.function_of(r) for r in code_refs(self.gog, v)})
            theirs = sorted({self.other.function_of(r) for r in code_refs(self.other, there)})
            if len(mine) == len(theirs) and f in mine and len(mine) <= 16:
                ev.add(theirs[mine.index(f)], f"strings {v:#x} {mine.index(f) + 1} of {len(mine)}")
                return

    def function(self, f, depth=0):
        """The other build's start of GOG's function f: (value, status, text). At depth 0 its call
        sites are matched inside their own functions' copies (found at depth 1)."""
        key = (f, depth > 0)
        if key in self.functions:
            return self.functions[key]
        if depth > 0 and (f, False) in self.functions:
            return self.functions[(f, False)]
        ev = Evidence()
        self.by_window(f, ev)
        self.by_vtables(f, ev)
        self.by_body(f, ev)
        if depth == 0 or ev.verdict()[1] != "verified":
            self.by_callers(f, ev, depth)
        if depth == 0 and ev.verdict()[1] != "verified":
            self.near(f, ev)
        if depth == 0 and ev.verdict()[1] != "verified":
            self.by_strings(f, ev)
        self.functions[key] = ev.verdict()
        return self.functions[key]

    def region(self, f, start):
        """Where the other build's copy of GOG's function f (found at start) can be: as long as
        GOG's, a quarter more and 256 bytes."""
        n = self.gog.next_start(f) - f
        return start, start + n + n // 4 + 256

    def site(self, va, ev):
        """An address inside a function: its function's copy, then the window inside it."""
        f = self.gog.function_of(va)
        if f is None or f == va:
            return
        start, status, _ = self.function(f)
        if start is None:
            return
        region = self.region(f, start)
        for loose in (False, True):
            for back in (0, 8, 16, 32):
                there, size = find_code(self.gog, self.other, va, before=back, region=region, loose=loose)
                if there is not None:
                    ev.add(there, f"local {back}+{size}{' loose' if loose else ''} in {f:#x}->{start:#x} ({status})")
                    return

    def near(self, va, ev, wanted=2):
        """Code before and after the address matched alone (anchors: the functions are laid out in
        the same order), and the address's window looked for only where each anchor puts it
        (within 64 bytes)."""
        found = 0
        for d in (-48, 48, -96, 96, -160, 160, -256, 256):
            anchor, _ = find_code(self.gog, self.other, va + d)
            if anchor is None:
                continue
            guess = anchor - d
            for back, loose in ((0, False), (8, False), (16, False), (0, True), (8, True), (16, True)):
                there, size = find_code(self.gog, self.other, va, before=back, region=(guess - 64, guess + 64),
                                        loose=loose)
                if there is not None:
                    ev.add(there, f"near {d:+d}{' loose' if loose else ''}")
                    found += 1
                    break
            if found >= wanted:
                return

    def code(self, va):
        if va in self.gog.calls() or va == self.gog.function_of(va) and va in self.gog.starts():
            return self.function(va)
        ev = Evidence()
        self.by_window(va, ev)
        self.site(va, ev)
        if ev.verdict()[1] != "verified":
            self.near(va, ev)
        return ev.verdict()

    # ---- data ----
    def refs(self, va, ev, limit=12):
        needle = struct.pack("<I", va)
        pos = self.gog.code.find(needle)
        tried = 0
        while pos >= 0 and tried < limit:
            tried += 1
            site = self.gog.code_va_of(pos)
            there, local = self.site_in(site, 0, back_list=(2, 8, 16, 32))
            if there is not None:
                ev.add(self.other.read32(there), f"refs {site:#x}" if local else f"refs-global {site:#x}")
            pos = self.gog.code.find(needle, pos + 1)

    def by_string(self, va, ev):
        """A string (narrow or wide, 6 characters or more) at va: the same one in the other's .rdata,
        starting where a string can (after a zero byte), and found once."""
        needle, wide = string_at(self.gog, va, least=3)
        there = find_string(self.other, needle, wide) if needle else None
        if there is not None:
            ev.add(there, f"string {'wide' if wide else 'narrow'} {len(needle)} bytes")
            mine, theirs = len(code_refs(self.gog, va)), len(code_refs(self.other, there))
            if mine and mine == theirs:
                ev.add(there, f"string-refs {mine} in each build's code")

    def data(self, va):
        ev = Evidence()
        slot = self.gog.iat().get(va)
        if slot:
            for there, imp in self.other.iat().items():
                if imp == slot:
                    ev.add(there, f"import {slot[0]}!{slot[1]}")
        rtti = self.gog.rtti(va)
        if rtti:
            found = self.other.vtables_of(*rtti)
            if len(found) == 1:
                ev.add(found[0], f"rtti {rtti[0]} {rtti[1]:#x}")
        if not slot and not rtti:
            self.by_string(va, ev)
        self.refs(va, ev)
        value, status, text = ev.verdict()
        if value is not None and not slot and not rtti and self.gog.section_of(va) == ".rdata":
            needle, _ = string_at(self.gog, va)
            n = len(needle) if needle else 4
            same = self.gog.read(va, n) == self.other.read(value, n)
            text += f"; content {n} bytes {'equal' if same else 'DIFFERENT'}"
            if not same:
                status = "unresolved"
                value = None
        return value, status, text

    def port(self, va):
        """(the other build's address or None, status, how it was found)."""
        if self.gog.in_code(va) and va not in self.gog.iat():
            return self.code(va)
        return self.data(va)


def port(gog, other, va):
    """(the other build's address or None, how it was found), for one GOG address."""
    value, status, text = Porter(gog, other).port(va)
    return value, f"{status}: {text}"


def code_spans(text):
    """[(start, end)] of C source outside comments and string or character literals."""
    spans, i, n, start = [], 0, len(text), 0
    while i < n:
        if text.startswith("/*", i) or text.startswith("//", i) or text[i] in "\"'":
            if i > start:
                spans.append((start, i))
            if text.startswith("/*", i):
                j = text.find("*/", i + 2)
                i = n if j < 0 else j + 2
            elif text.startswith("//", i):
                j = text.find("\n", i)
                i = n if j < 0 else j
            else:
                j = i + 1
                while j < n and text[j] != text[i]:
                    j += 2 if text[j] == "\\" else 1
                i = j + 1
            start = i
        else:
            i += 1
    if start < n:
        spans.append((start, n))
    return spans


ADDRESS = re.compile(r"\b0x([0-9a-fA-F]{6,8})([uUlL]*)\b")


def addresses_in(text, code_only=False):
    """[(start, end, value)] of the hex constants LOW..HIGH in C source (with code_only, only those
    outside comments and literals)."""
    spans = code_spans(text) if code_only else [(0, len(text))]
    out = []
    for a, b in spans:
        for m in ADDRESS.finditer(text, a, b):
            v = int(m.group(1), 16)
            if LOW <= v < HIGH:
                out.append((m.start(1) - 2, m.end(1), v))
    return out


def source_addresses(folder, code_only=False):
    found = set()
    for name in sorted(os.listdir(folder)):
        if name.endswith(".c"):
            with open(os.path.join(folder, name), encoding="utf-8") as f:
                text = f.read()
            found.update(v for _, _, v in addresses_in(text, code_only))
    return sorted(found)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("gog")
    ap.add_argument("other")
    ap.add_argument("addresses", nargs="*")
    ap.add_argument("--source")
    ap.add_argument("--code-only", action="store_true")
    ap.add_argument("--table")
    a = ap.parse_args(argv)
    gog = Image(open(a.gog, "rb").read())
    other = Image(open(a.other, "rb").read())
    addresses = [int(x, 16) for x in a.addresses]
    addresses += source_addresses(a.source, a.code_only) if a.source else []
    porter = Porter(gog, other)
    counts = {}
    lines = []
    for va in addresses:
        value, status, text = porter.port(va)
        counts[status] = counts.get(status, 0) + 1
        line = f"{va:#010x} {value:#010x} {status} {text}" if value is not None else f"{va:#010x} - {status} {text}"
        print(line, flush=True)
        lines.append(line)
    if a.table:
        with open(a.table, "w") as f:
            f.write("\n".join(lines) + "\n")
    print(", ".join(f"{n} {s}" for s, n in sorted(counts.items())) + f" of {len(addresses)}", file=sys.stderr)
    return 1 if counts.get("unresolved") else 0


if __name__ == "__main__":
    sys.exit(main())
