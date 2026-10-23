#!/usr/bin/env python3
"""Reads the files out of an Xbox 360 STFS package (CON, LIVE or PIRS), such as Torchlight's XBLA
package, written from the format's public description (the Free60 wiki's STFS page).

    stfs.py PACKAGE              lists the package's files
    stfs.py PACKAGE OUT [NAME..] extracts every file (or the NAMEs) into OUT

The layout read:
- the header: magic at 0, header size at 0x340, content type at 0x344, title ID at 0x360 (big
  endian); the STFS volume descriptor at 0x379: its block separation byte at +2, the file table's
  block count (little endian u16) at +3 and first block (little endian u24) at +5, then the top
  hash, then the total and free block counts (big endian);
- blocks of 0x1000 bytes from the header's end rounded up to 0x1000; each 0xAA data blocks follow a
  hash table, each 0xAA tables a level 1 table, each 0xAA level 1 tables a level 2 one. A package
  whose rounded header is 0xB000 (or whose separation's bit 0 is set) keeps one copy of each table,
  else two;
- the file table: 0x40 byte entries (name 0x28, flags at 0x28: the name's length in bits 0-5,
  contiguous bit 6, folder bit 7; block count u24 LE at 0x29, first block u24 LE at 0x2F, the
  parent's index (big endian i16, -1 the root) at 0x32, size big endian u32 at 0x34);
- a file's next block, unless contiguous: the level 0 hash table's entry for its block (0x18 bytes:
  SHA-1, a status byte, the next block big endian u24).
"""
import os
import struct
import sys

MAGICS = (b"CON ", b"LIVE", b"PIRS")
BLOCK = 0x1000
PER_TABLE = 0xAA


class Package:
    def __init__(self, path):
        self.f = open(path, "rb")
        head = self.f.read(0x3A0)
        if head[:4] not in MAGICS:
            raise ValueError("not an Xbox 360 package (CON, LIVE or PIRS)")
        self.magic = head[:4]
        self.header_size = struct.unpack_from(">I", head, 0x340)[0]
        self.content_type = struct.unpack_from(">I", head, 0x344)[0]
        self.title_id = struct.unpack_from(">I", head, 0x360)[0]
        v = 0x379
        self.separation = head[v + 2]
        self.table_blocks = struct.unpack_from("<H", head, v + 3)[0]
        self.table_first = int.from_bytes(head[v + 5:v + 8], "little")
        self.base = (self.header_size + 0xFFF) & ~0xFFF
        self.shift = 0 if self.base == 0xB000 or self.separation & 1 else 1
        self.files = self._read_table()

    def close(self):
        self.f.close()

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()

    # ---- blocks ----

    def data_block(self, n):
        """The package's block number of data block n (the hash tables before it skipped)."""
        s = self.shift
        b = n + (((n + PER_TABLE) // PER_TABLE) << s)
        if n < PER_TABLE:
            return b
        b += ((n + PER_TABLE ** 2) // PER_TABLE ** 2) << s
        if n < PER_TABLE ** 2:
            return b
        return b + (1 << s)

    def hash_block(self, n):
        """The package's block number of the level 0 hash table that covers data block n."""
        s = self.shift
        step = PER_TABLE + (1 << s)
        if n < PER_TABLE:
            return 0
        b = (n // PER_TABLE) * step
        b += ((n // PER_TABLE ** 2) + 1) << s
        if n < PER_TABLE ** 2:
            return b
        return b + (1 << s)

    def read_block(self, block):
        self.f.seek(self.base + block * BLOCK)
        return self.f.read(BLOCK)

    def next_block(self, n):
        self.f.seek(self.base + self.hash_block(n) * BLOCK + (n % PER_TABLE) * 0x18 + 0x15)
        return int.from_bytes(self.f.read(3), "big")

    def blocks(self, first, count, contiguous):
        n = first
        for _ in range(count):
            yield n
            n = n + 1 if contiguous else self.next_block(n)

    # ---- the file table ----

    def _read_table(self):
        entries = []
        for n in self.blocks(self.table_first, self.table_blocks, False):
            data = self.read_block(self.data_block(n))
            for o in range(0, BLOCK, 0x40):
                e = data[o:o + 0x40]
                length = e[0x28] & 0x3F
                if not length:
                    continue
                entries.append({
                    "name": e[:length].decode("latin-1"),
                    "folder": bool(e[0x28] & 0x80),
                    "contiguous": bool(e[0x28] & 0x40),
                    "blocks": int.from_bytes(e[0x29:0x2C], "little"),
                    "first": int.from_bytes(e[0x2F:0x32], "little"),
                    "parent": struct.unpack_from(">h", e, 0x32)[0],
                    "size": struct.unpack_from(">I", e, 0x34)[0],
                })
        for e in entries:
            parts, p, seen = [e["name"]], e["parent"], 0
            while 0 <= p < len(entries) and seen < 64:
                parts.append(entries[p]["name"])
                p, seen = entries[p]["parent"], seen + 1
            e["path"] = "/".join(reversed(parts))
        return [e for e in entries if not e["folder"]]

    def find(self, path):
        low = path.replace("\\", "/").lower()
        return next((e for e in self.files if e["path"].lower() == low), None)

    def read(self, entry, out=None):
        """The file's bytes, or written to the open file out."""
        left, chunks = entry["size"], []
        for n in self.blocks(entry["first"], entry["blocks"], entry["contiguous"]):
            if left <= 0:
                break
            data = self.read_block(self.data_block(n))[:min(BLOCK, left)]
            left -= len(data)
            if out:
                out.write(data)
            else:
                chunks.append(data)
        if left > 0:
            raise ValueError(f"{entry['path']}: the package ends before the file")
        return None if out else b"".join(chunks)

    def extract(self, entry, dest):
        path = os.path.join(dest, *entry["path"].split("/"))
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "wb") as f:
            self.read(entry, f)
        return path


def main(argv):
    if not argv or len(argv) == 2 and False:
        sys.exit(__doc__)
    with Package(argv[0]) as p:
        print(f"{p.magic.decode().strip()} title {p.title_id:08X} type {p.content_type:08X}, {len(p.files)} files")
        if len(argv) == 1:
            for e in p.files:
                print(f"{e['size']:>12} {e['path']}")
            return 0
        names = argv[2:]
        for e in p.files:
            if not names or e["path"] in names:
                print(p.extract(e, argv[1]))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
