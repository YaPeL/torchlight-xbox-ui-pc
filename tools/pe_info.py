#!/usr/bin/env python3
"""Read a Windows PE file (an .exe or .dll of the user's PC install): sections, imports, exports.

pe_info.py FILE [--imports] [--exports] [--grep TEXT]
Without a flag, prints the header facts and the sections.
"""
import argparse
import struct
import sys


class PE:
    def __init__(self, data):
        self.data = data
        if data[:2] != b"MZ":
            raise ValueError("not a PE file (no MZ)")
        pe = struct.unpack_from("<I", data, 0x3C)[0]
        if data[pe:pe + 4] != b"PE\0\0":
            raise ValueError("not a PE file (no PE signature)")
        self.machine, nsec, self.timestamp = struct.unpack_from("<HHI", data, pe + 4)
        opt_size = struct.unpack_from("<H", data, pe + 20)[0]
        opt = pe + 24
        magic = struct.unpack_from("<H", data, opt)[0]
        self.pe32_plus = magic == 0x20B
        self.image_base = (struct.unpack_from("<Q", data, opt + 24)[0] if self.pe32_plus
                           else struct.unpack_from("<I", data, opt + 28)[0])
        dirs = opt + (112 if self.pe32_plus else 96)
        ndirs = struct.unpack_from("<I", data, dirs - 4)[0]
        self.dirs = [struct.unpack_from("<II", data, dirs + 8 * i) for i in range(min(ndirs, 16))]
        self.sections = []
        for i in range(nsec):
            o = opt + opt_size + 40 * i
            name = data[o:o + 8].rstrip(b"\0").decode("latin1")
            vsize, va, rsize, raw = struct.unpack_from("<IIII", data, o + 8)
            self.sections.append((name, va, vsize, raw, rsize))

    def offset(self, rva):
        for _, va, vsize, raw, rsize in self.sections:
            if va <= rva < va + max(vsize, rsize):
                return raw + rva - va
        return None

    def cstr(self, rva):
        o = self.offset(rva)
        return self.data[o:self.data.index(b"\0", o)].decode("latin1") if o is not None else ""

    def exports(self):
        if len(self.dirs) < 1 or not self.dirs[0][0]:
            return []
        o = self.offset(self.dirs[0][0])
        ordinal_base, nfuncs, nnames, funcs, names, ords = struct.unpack_from("<IIIIII", self.data, o + 16)
        out = []
        for i in range(nnames):
            name = self.cstr(struct.unpack_from("<I", self.data, self.offset(names) + 4 * i)[0])
            index = struct.unpack_from("<H", self.data, self.offset(ords) + 2 * i)[0]
            out.append((name, ordinal_base + index))
        return out

    def imports(self):
        """{dll name: [function name or '#ordinal']}"""
        out = {}
        if len(self.dirs) < 2 or not self.dirs[1][0]:
            return out
        o = self.offset(self.dirs[1][0])
        width = 8 if self.pe32_plus else 4
        flag = 1 << (width * 8 - 1)
        while True:
            lookup, _, _, name, first = struct.unpack_from("<IIIII", self.data, o)
            if not (lookup or name or first):
                break
            dll = self.cstr(name)
            funcs = []
            t = self.offset(lookup or first)
            while True:
                v = struct.unpack_from("<Q" if self.pe32_plus else "<I", self.data, t)[0]
                if not v:
                    break
                funcs.append(f"#{v & 0xFFFF}" if v & flag else self.cstr((v & 0x7FFFFFFF) + 2))
                t += width
            out[dll] = funcs
            o += 20
        return out


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("file")
    ap.add_argument("--imports", action="store_true")
    ap.add_argument("--exports", action="store_true")
    ap.add_argument("--grep", default="")
    a = ap.parse_args()
    pe = PE(open(a.file, "rb").read())
    if a.imports:
        for dll, funcs in pe.imports().items():
            for f in funcs:
                if a.grep in dll or a.grep in f:
                    print(f"{dll}\t{f}")
    if a.exports:
        for name, ordinal in pe.exports():
            if a.grep in name:
                print(f"{ordinal}\t{name}")
    if not (a.imports or a.exports):
        print(f"machine 0x{pe.machine:04x} timestamp {pe.timestamp} image base 0x{pe.image_base:x}")
        for name, va, vsize, raw, rsize in pe.sections:
            print(f"{name:8} va 0x{va:08x} vsize 0x{vsize:x} raw 0x{raw:x} rsize 0x{rsize:x}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
