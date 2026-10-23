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
