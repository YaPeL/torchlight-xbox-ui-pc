"""The C++ installer's readers against the Python tools, on synthetic data (tests/test_tools.py's
builders). Skipped unless X360UI_INSTALLER names the built x360ui-install (CI builds it)."""
import os
import struct
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "tools"))
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "tools", "xex"))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import stfs  # noqa: E402
from test_tools import stfs_package  # noqa: E402

BINARY = os.environ.get("X360UI_INSTALLER")
TOOLS = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "tools")


def run(*args):
    return subprocess.run(list(args), check=True, capture_output=True).stdout


@unittest.skipUnless(BINARY and os.path.isfile(BINARY), "X360UI_INSTALLER not set")
class InstallerCppTest(unittest.TestCase):
    def test_stfs_list_and_extract_as_python(self):
        files = {"pak.zip": b"zip" * 100, "default.xex": bytes(range(256)) * 37, "a.png": b""}
        for contiguous in (True, False):
            with tempfile.TemporaryDirectory() as d:
                pkg = os.path.join(d, "pkg")
                with open(pkg, "wb") as f:
                    f.write(stfs_package(files, contiguous=contiguous))
                self.assertEqual(run(BINARY, "dev", "stfs", pkg), run(sys.executable, os.path.join(TOOLS, "stfs.py"), pkg))
                run(BINARY, "dev", "stfs", pkg, os.path.join(d, "cpp"))
                run(sys.executable, os.path.join(TOOLS, "stfs.py"), pkg, os.path.join(d, "py"))
                for name in files:
                    with open(os.path.join(d, "cpp", name), "rb") as a, open(os.path.join(d, "py", name), "rb") as b:
                        self.assertEqual(a.read(), b.read(), name)

    def test_aes_as_python(self):
        import aes128
        key, iv = bytes(range(16)), bytes(range(16, 32))
        data = bytes((i * 31) % 256 for i in range(16 * 40))
        with tempfile.TemporaryDirectory() as d:
            src, out = os.path.join(d, "in"), os.path.join(d, "out")
            with open(src, "wb") as f:
                f.write(data)
            run(BINARY, "dev", "aes-cbc", key.hex(), iv.hex(), src, out)
            with open(out, "rb") as f:
                self.assertEqual(f.read(), aes128.cbc_decrypt(key, iv, data))

    def test_lzx_as_python_on_random_streams(self):
        """Seeded random streams, biased to valid block headers (verbatim, aligned, uncompressed),
        through both decoders: the same bytes, or both an error."""
        import random
        from lzx_decompress import LZXDecoder
        rng = random.Random(1234)
        same = 0
        with tempfile.TemporaryDirectory() as d:
            kinds = {1: 0, 2: 0, 3: 0}
            for k in range(600):
                bits, size = rng.choice((15, 16, 17)), rng.randint(1, 20000)
                # The first 16-bit word is little endian: its bit 15 (data[1]'s bit 7) is the E8 flag,
                # kept 0, then the block type in data[1]'s bits 6..4: verbatim, aligned, uncompressed.
                kind = rng.choice((1, 2, 3))
                data = bytes([rng.randrange(256), (kind << 4) | rng.randrange(16)]) + bytes(rng.randrange(256) for _ in range(rng.randint(8, 3000)))
                src, out = os.path.join(d, "in"), os.path.join(d, "out")
                with open(src, "wb") as f:
                    f.write(data)
                try:
                    want = LZXDecoder(bits).decompress(data, size)
                except Exception:
                    want = None
                r = subprocess.run([BINARY, "dev", "lzx", str(bits), str(size), src, out], capture_output=True)
                if want is None:
                    self.assertNotEqual(r.returncode, 0, f"stream {k}: Python failed, C++ did not")
                else:
                    self.assertEqual(r.returncode, 0, f"stream {k}: {r.stderr!r}")
                    with open(out, "rb") as f:
                        self.assertEqual(f.read(), want, f"stream {k}")
                    same += 1
                    kinds[kind] += 1
        self.assertGreater(same, 250)   # most streams decode, so the test means something
        self.assertTrue(all(n > 50 for n in kinds.values()), kinds)   # each block type decoded

    def test_stfs_names_with_a_nul_refused(self):
        with tempfile.TemporaryDirectory() as d:
            pkg = os.path.join(d, "pkg")
            with open(pkg, "wb") as f:
                f.write(stfs_package({"pak.zip": b"good", "pak.zip\0": b"bad"}))   # the good one first: the bad would overwrite it
            r = subprocess.run([BINARY, "dev", "stfs", pkg, os.path.join(d, "out")], capture_output=True)
            self.assertNotEqual(r.returncode, 0)
            with open(os.path.join(d, "out", "pak.zip"), "rb") as f:
                self.assertEqual(f.read(), b"good")

    def test_refuses_what_is_not_a_package(self):
        with tempfile.TemporaryDirectory() as d:
            path = os.path.join(d, "x")
            with open(path, "wb") as f:
                f.write(b"MZ" + bytes(0x400))
            r = subprocess.run([BINARY, "dev", "stfs", path], capture_output=True)
            self.assertEqual(r.returncode, 1)
            self.assertIn(b"not an Xbox 360 package", r.stderr)


if __name__ == "__main__":
    unittest.main()
